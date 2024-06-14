#pragma once

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"
#include "node.h"

#include "tiny_gltf.h"

#include "stb_image.h"

namespace fs = std::filesystem;
namespace Utils
{
	bool loadGeometryFromObj(const fs::path& path) {
		//std::vector<VertexAttributes> vertexData;
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		std::string warn;
		std::string err;

		// Call the core loading procedure of TinyOBJLoader
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str());

		// Check errors
		if (!warn.empty()) {
			std::cout << warn << std::endl;
		}

		if (!err.empty()) {
			std::cerr << err << std::endl;
		}

		if (!ret) {
			return false;
		}

		// Filling in vertexData:
		//vertexData.clear();
		std::vector<Vertex> vertices;
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex = {};

				vertex.position = {
					attrib.vertices[3 * index.vertex_index + 0],
					-attrib.vertices[3 * index.vertex_index + 2], // Add a minus to avoid mirroring
					attrib.vertices[3 * index.vertex_index + 1]
				};

				// Also apply the transform to normals!!
				if (!attrib.normals.empty()) {
					vertex.normal = {
						attrib.normals[3 * index.normal_index + 0],
						-attrib.normals[3 * index.normal_index + 2],
						attrib.normals[3 * index.normal_index + 1]
					};
				}

				if (!attrib.texcoords.empty()) {
					vertex.uv = {
						attrib.texcoords[2 * index.texcoord_index + 0],
						1- attrib.texcoords[2 * index.texcoord_index + 1]
					};
				}

				vertices.push_back(vertex);
			}
		}


		Mesh* mesh = new Mesh();
		mesh->setVertices(vertices);
		MeshManager::getInstance().add("obj_1", mesh);

		return true;
	}

	// Auxiliary function for loadTexture
	static void writeMipMaps(
		Texture texture,
		Extent3D textureSize,
		uint32_t mipLevelCount,
		const unsigned char* pixelData)
	{
		Queue queue = Context::getInstance().getDevice().getQueue();

		// Arguments telling which part of the texture we upload to
		ImageCopyTexture destination;
		destination.texture = texture;
		destination.origin = { 0, 0, 0 };
		destination.aspect = TextureAspect::All;

		// Arguments telling how the C++ side pixel memory is laid out
		TextureDataLayout source;
		source.offset = 0;

		// Create image data
		Extent3D mipLevelSize = textureSize;
		std::vector<unsigned char> previousLevelPixels;
		Extent3D previousMipLevelSize;
		for (uint32_t level = 0; level < mipLevelCount; ++level) {
			// Pixel data for the current level
			std::vector<unsigned char> pixels(4 * mipLevelSize.width * mipLevelSize.height);
			if (level == 0) {
				// We cannot really avoid this copy since we need this
				// in previousLevelPixels at the next iteration
				memcpy(pixels.data(), pixelData, pixels.size());
			}
			else {
				// Create mip level data
				for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
					for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
						unsigned char* p = &pixels[4 * (j * mipLevelSize.width + i)];
						// Get the corresponding 4 pixels from the previous level
						unsigned char* p00 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 0))];
						unsigned char* p01 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 1))];
						unsigned char* p10 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 0))];
						unsigned char* p11 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 1))];
						// Average
						p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
						p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
						p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
						p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / 4;
					}
				}
			}

			// Upload data to the GPU texture
			destination.mipLevel = level;
			source.bytesPerRow = 4 * mipLevelSize.width;
			source.rowsPerImage = mipLevelSize.height;
			queue.writeTexture(destination, pixels.data(), pixels.size(), source, mipLevelSize);

			previousLevelPixels = std::move(pixels);
			previousMipLevelSize = mipLevelSize;
			mipLevelSize.width /= 2;
			mipLevelSize.height /= 2;
		}

		queue.release();
	}

	static uint32_t bit_width(uint32_t m) {
		if (m == 0) return 0;
		else { uint32_t w = 0; while (m >>= 1) ++w; return w; }
	}

	Texture loadTexture(const fs::path& path, TextureView* pTextureView = nullptr) {
		int width, height, channels;
		unsigned char* pixelData = stbi_load(path.string().c_str(), &width, &height, &channels, 4 /* force 4 channels */);
		// If data is null, loading failed.
		if (nullptr == pixelData) return nullptr;

		// Use the width, height, channels and data variables here
		TextureDescriptor textureDesc;
		textureDesc.dimension = TextureDimension::_2D;
		textureDesc.format = TextureFormat::RGBA8Unorm; // by convention for bmp, png and jpg file. Be careful with other formats.
		textureDesc.size = { (unsigned int)width, (unsigned int)height, 1 };
		textureDesc.mipLevelCount = bit_width(std::max(textureDesc.size.width, textureDesc.size.height));
		textureDesc.sampleCount = 1;
		textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
		textureDesc.viewFormatCount = 0;
		textureDesc.viewFormats = nullptr;
		Texture texture = Context::getInstance().getDevice().createTexture(textureDesc);

		// Upload data to the GPU texture
		writeMipMaps(texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

		stbi_image_free(pixelData);
		// (Do not use data after this)

		if (pTextureView) {
			TextureViewDescriptor textureViewDesc;
			textureViewDesc.aspect = TextureAspect::All;
			textureViewDesc.baseArrayLayer = 0;
			textureViewDesc.arrayLayerCount = 1;
			textureViewDesc.baseMipLevel = 0;
			textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
			textureViewDesc.dimension = TextureViewDimension::_2D;
			textureViewDesc.format = textureDesc.format;
			*pTextureView = texture.createView(textureViewDesc);
		}

		return texture;
	}

	wgpu::Texture CreateWhiteTexture(TextureView* pTextureView) {

		wgpu::Extent3D textureSize = { 1, 1, 1 };
		// Define the texture descriptor
		wgpu::TextureDescriptor textureDesc = {};
		textureDesc.size = textureSize;
		textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
		textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
		textureDesc.dimension = wgpu::TextureDimension::_2D;
		textureDesc.mipLevelCount = 1;
		textureDesc.sampleCount = 1;

		// Create the texture
		Texture texture = Context::getInstance().getDevice().createTexture(textureDesc);

		// Define the white pixel data
		uint8_t whitePixel[4] = { 255, 255, 255, 255 };

		// Define the texture data layout
		wgpu::TextureDataLayout textureDataLayout = {};
		textureDataLayout.offset = 0;
		textureDataLayout.bytesPerRow = 4;
		textureDataLayout.rowsPerImage = 1;

		// Define the texture copy view
		wgpu::ImageCopyTexture imageCopyTexture = {};
		imageCopyTexture.texture = texture;
		imageCopyTexture.origin = { 0, 0, 0 };
		imageCopyTexture.mipLevel = 0;

		// Write the white pixel data to the texture

		Context::getInstance().getDevice().getQueue().writeTexture(imageCopyTexture, whitePixel, sizeof(whitePixel), textureDataLayout, textureSize);

		if (pTextureView) {
			TextureViewDescriptor textureViewDesc;
			textureViewDesc.aspect = TextureAspect::All;
			textureViewDesc.baseArrayLayer = 0;
			textureViewDesc.arrayLayerCount = 1;
			textureViewDesc.baseMipLevel = 0;
			textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
			textureViewDesc.dimension = TextureViewDimension::_2D;
			textureViewDesc.format = textureDesc.format;
			*pTextureView = texture.createView(textureViewDesc);
		}

		return texture;
	}


	std::string loadFile(const std::filesystem::path& path) {
		std::ifstream file(path);
		if (!file.is_open()) {
			std::cerr << path.c_str() << " not found ! " << std::endl;
		}
		file.seekg(0, std::ios::end);
		size_t size = file.tellg();
		std::string str = std::string(size, ' ');
		file.seekg(0);
		file.read(str.data(), size);
		return str;
	};

	Sampler createDefaultSampler()
	{
		SamplerDescriptor samplerDesc;
		samplerDesc.addressModeU = AddressMode::Repeat;
		samplerDesc.addressModeV = AddressMode::Repeat;
		samplerDesc.addressModeW = AddressMode::Repeat;
		samplerDesc.magFilter = FilterMode::Linear;
		samplerDesc.minFilter = FilterMode::Linear;
		samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
		samplerDesc.lodMinClamp = 0.0f;
		samplerDesc.lodMaxClamp = 8.0f;
		samplerDesc.compare = CompareFunction::Undefined;
		samplerDesc.maxAnisotropy = 1;
		return Context::getInstance().getDevice().createSampler(samplerDesc);
	}


	void loadMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, Issam::Node& node)
	{
		for (const auto& primitive : mesh.primitives) {
			std::vector<Vertex> vertices;
			std::vector<uint16_t> indices;
			// Process vertex attributes
			const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
			//if (posAccessor.bufferView == -1) return nullptr;
			const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
			const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

			const tinygltf::Accessor* normAccessor = nullptr;
			const tinygltf::BufferView* normBufferView = nullptr;
			const tinygltf::Buffer* normBuffer = nullptr;

			const tinygltf::Accessor* uvAccessor = nullptr;
			const tinygltf::BufferView* uvBufferView = nullptr;
			const tinygltf::Buffer* uvBuffer = nullptr;

			const tinygltf::Accessor* tangentAccessor = nullptr;
			const tinygltf::BufferView* tangentBufferView = nullptr;
			const tinygltf::Buffer* tangentBuffer = nullptr;

			if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
				normAccessor = &model.accessors[primitive.attributes.find("NORMAL")->second];
				normBufferView = &model.bufferViews[normAccessor->bufferView];
				normBuffer = &model.buffers[normBufferView->buffer];
			}

			if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
				uvAccessor = &model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
				uvBufferView = &model.bufferViews[uvAccessor->bufferView];
				uvBuffer = &model.buffers[uvBufferView->buffer];
			}

			if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
				tangentAccessor = &model.accessors[primitive.attributes.find("TANGENT")->second];
				tangentBufferView = &model.bufferViews[tangentAccessor->bufferView];
				tangentBuffer = &model.buffers[tangentBufferView->buffer];
			}

			for (size_t i = 0; i < posAccessor.count; ++i) {
				Vertex vertex = {};

				size_t posIndex = i * posAccessor.ByteStride(posBufferView) + posAccessor.byteOffset + posBufferView.byteOffset;
				vertex.position = glm::vec3(
					*reinterpret_cast<const float*>(&posBuffer.data[posIndex + 0 * sizeof(float)]),
					*reinterpret_cast<const float*>(&posBuffer.data[posIndex + 1 * sizeof(float)]),
					*reinterpret_cast<const float*>(&posBuffer.data[posIndex + 2 * sizeof(float)]));

				if (normAccessor) {
					size_t normIndex = i * normAccessor->ByteStride(*normBufferView) + normAccessor->byteOffset + normBufferView->byteOffset;
					vertex.normal = glm::vec3(
						*reinterpret_cast<const float*>(&normBuffer->data[normIndex + 0 * sizeof(float)]),
						*reinterpret_cast<const float*>(&normBuffer->data[normIndex + 1 * sizeof(float)]),
						*reinterpret_cast<const float*>(&normBuffer->data[normIndex + 2 * sizeof(float)]));
				}

				if (uvAccessor) {
					size_t uvIndex = i * uvAccessor->ByteStride(*uvBufferView) + uvAccessor->byteOffset + uvBufferView->byteOffset;
					vertex.uv = glm::vec2(
						*reinterpret_cast<const float*>(&uvBuffer->data[uvIndex + 0 * sizeof(float)]),
						*reinterpret_cast<const float*>(&uvBuffer->data[uvIndex + 1 * sizeof(float)]));
				}

				if (tangentAccessor) {
					size_t tangentIndex = i * tangentAccessor->ByteStride(*tangentBufferView) + tangentAccessor->byteOffset + tangentBufferView->byteOffset;
					vertex.tangent = glm::vec3(
						*reinterpret_cast<const float*>(&tangentBuffer->data[tangentIndex + 0 * sizeof(float)]),
						*reinterpret_cast<const float*>(&tangentBuffer->data[tangentIndex + 1 * sizeof(float)]),
						*reinterpret_cast<const float*>(&tangentBuffer->data[tangentIndex + 2 * sizeof(float)]));
				}

				vertices.push_back(vertex);
			}
			Mesh* myMesh = new Mesh();
			myMesh->setVertices(vertices);

			if (primitive.indices != -1)
			{
				// Process indices
				const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
				const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
				const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

				for (size_t i = 0; i < indexAccessor.count; ++i) {
					size_t index = i * indexAccessor.ByteStride(indexBufferView) + indexAccessor.byteOffset + indexBufferView.byteOffset;
					uint32_t indexValue;
					switch (indexAccessor.componentType) {
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						indexValue = *reinterpret_cast<const uint8_t*>(&indexBuffer.data[index]);
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						indexValue = *reinterpret_cast<const uint16_t*>(&indexBuffer.data[index]);
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						assert(false); //NOT SUPPORTED
						indexValue = *reinterpret_cast<const uint32_t*>(&indexBuffer.data[index]);
						break;
					default:
						throw std::runtime_error("Unsupported index component type");
					}
					indices.push_back(indexValue);
				}
				myMesh->setIndices(indices);
			}

			static int meshId = 0;
			node.meshId = "mesh_" + std::to_string(meshId++);
			MeshManager::getInstance().add(node.meshId, myMesh);
			
		}
	}

	void loadNode(const tinygltf::Model& model, const tinygltf::Node& gltfNode, Issam::Node& node) {
		node.name = gltfNode.name;
		/*node.transform = glm::mat4(1.0f);
		if (!gltfNode.translation.empty()) {
			node.transform = glm::translate(node.transform, glm::vec3(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]));
		}
		if (!gltfNode.rotation.empty()) {
			glm::quat quatRotation = glm::quat(gltfNode.rotation[3], gltfNode.rotation[0], gltfNode.rotation[1], gltfNode.rotation[2]);
			node.transform *= glm::mat4_cast(quatRotation);
		}
		if (!gltfNode.scale.empty()) {
			node.transform = glm::scale(node.transform, glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]));
		}*/
		

		const glm::vec3 T =
			gltfNode.translation.empty()
			? glm::vec3(0.0)
			: glm::make_vec3(gltfNode.translation.data());

		const glm::quat R =
			gltfNode.rotation.empty()
			? glm::quat()
			: glm::make_quat(gltfNode.rotation.data());

		const glm::vec3 S =
			gltfNode.scale.empty()
			? glm::vec3(1.0)
			: glm::make_vec3(gltfNode.scale.data());

		node.transform = glm::translate(glm::mat4(1.0), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0), S);
		
		if (!gltfNode.matrix.empty()) {
			/*glm::mat4 mat;
			for (int i = 0; i < 16; ++i) {
				mat[i / 4][i % 4] = static_cast<float>(gltfNode.matrix[i]);
			}*/
			node.transform = glm::make_mat4(gltfNode.matrix.data());;
		}

		if (gltfNode.mesh >= 0) {
			loadMesh(model, model.meshes[gltfNode.mesh], node);
		}

		for (int childIndex : gltfNode.children) {
			Issam::Node childNode;
			loadNode(model, model.nodes[childIndex], childNode);
			node.children.push_back(childNode);
		}
	}



	std::vector<Issam::Node> LoadGLTF(const std::string& filepath) {
		std::vector<Issam::Node> scene;
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
		if (!warn.empty()) {
			std::cout << "WARN: " << warn << std::endl;
		}

		if (!err.empty()) {
			std::cerr << "ERR: " << err << std::endl;
		}

		if (!ret) {
			return scene;
		}

		
		for (const auto& sceneNodeIndex : model.scenes[model.defaultScene].nodes) {
			Issam::Node rootNode;
			loadNode(model, model.nodes[sceneNodeIndex], rootNode);
			scene.push_back(rootNode);
		}

		return scene;
	}
}