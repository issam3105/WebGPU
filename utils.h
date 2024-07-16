#pragma once

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>


#include "context.h"
#include "scene.h"
#include "material.h"

#include "tiny_gltf.h"

#include "stb_image.h"

namespace fs = std::filesystem;
namespace Utils
{

	entt::entity createBoundingBox(Issam::Scene* scene, const vec3& point1, const vec3& point2) {
		std::vector<Vertex> vertices(8);
		//std::vector<uint16_t> indices = {
		//	0, 1, 2, 2, 3, 0, // Front face
		//	4, 5, 6, 6, 7, 4, // Back face
		//	0, 1, 5, 5, 4, 0, // Bottom face
		//	2, 3, 7, 7, 6, 2, // Top face
		//	0, 3, 7, 7, 4, 0, // Left face
		//	1, 2, 6, 6, 5, 1  // Right face
		//};
		std::vector<uint16_t> indices = {
		   0, 1, 1, 2, 2, 3, 3, 0, // Bottom face
		   4, 5, 5, 6, 6, 7, 7, 4, // Top face
		   0, 4, 1, 5, 2, 6, 3, 7  // Vertical edges
		};


		vec3 minPoint = glm::min(point1, point2);
		vec3 maxPoint = glm::max(point1, point2);

		vertices[0].position = vec3(minPoint.x, minPoint.y, minPoint.z);
		vertices[1].position = vec3(maxPoint.x, minPoint.y, minPoint.z);
		vertices[2].position = vec3(maxPoint.x, maxPoint.y, minPoint.z);
		vertices[3].position = vec3(minPoint.x, maxPoint.y, minPoint.z);

		vertices[4].position = vec3(minPoint.x, minPoint.y, maxPoint.z);
		vertices[5].position = vec3(maxPoint.x, minPoint.y, maxPoint.z);
		vertices[6].position = vec3(maxPoint.x, maxPoint.y, maxPoint.z);
		vertices[7].position = vec3(minPoint.x, maxPoint.y, maxPoint.z);

		MeshPtr cubeMesh = std::make_shared<Mesh>();

		cubeMesh->setVertices(vertices);
		cubeMesh->setIndices(indices);

		entt::entity cubeEntity = scene->addEntity();
		Issam::MeshRenderer meshRenderer;
		meshRenderer.mesh = cubeMesh;
		Material* material = new Material();
		material->setAttribute("unlitMaterialModel", "colorFactor", glm::vec4(1.0, 0.0, 1.0, 1.0));
		meshRenderer.material = material;
		scene->addComponent<Issam::MeshRenderer>(cubeEntity, meshRenderer);

		Issam::Filters filters;
		filters.add("debug");
		scene->addComponent<Issam::Filters>(cubeEntity, filters);

		return cubeEntity;
	}

	entt::entity addLine(Issam::Scene* scene, glm::vec3 start, glm::vec3 end, glm::vec4 color) {
		std::vector<Vertex> vertices;
		vertices.push_back({ start});
		vertices.push_back({ end });
		std::vector<uint16_t> indices;
		indices.push_back(0);
		indices.push_back(1);

		MeshPtr lineMesh =std::make_shared<Mesh>();
		lineMesh->setVertices(vertices);
		lineMesh->setIndices(indices);

		Issam::MeshRenderer meshRenderer;
		meshRenderer.mesh = lineMesh;
		Material* material = new Material();
		material->setAttribute("unlitMaterialModel", "colorFactor", color);
		meshRenderer.material = material;

		entt::entity lineEntity = scene->addEntity();
		scene->addComponent<Issam::MeshRenderer>(lineEntity, meshRenderer);

		Issam::Filters filters;
		filters.add("debug");
		scene->addComponent<Issam::Filters>(lineEntity, filters);
		return lineEntity;
	}

	entt::entity addAxes(Issam::Scene* scene)
	{
		glm::vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
		glm::vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
		glm::vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);

		// Add X axis (red)
		entt::entity x_axis = addLine(scene, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), red);

		// Add Y axis (green)
		entt::entity y_axis= addLine(scene, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), green);

		// Add Z axis (blue)
		entt::entity z_axis =addLine(scene, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), blue);

		entt::entity axes = scene->addEntity();
		scene->addChild(axes, x_axis);
		scene->addChild(axes, y_axis);
		scene->addChild(axes, z_axis);
		return axes;
	}

	// Auxiliary function for loadTexture
	static void writeMipMaps(
		Texture texture,
		Extent3D textureSize,
		uint32_t mipLevelCount,
		const unsigned char* pixelData)
	{
		Queue queue = Context::getInstance().getDevice().GetQueue();

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
			queue.WriteTexture(&destination, pixels.data(), pixels.size(), &source, &mipLevelSize);

			previousLevelPixels = std::move(pixels);
			previousMipLevelSize = mipLevelSize;
			mipLevelSize.width /= 2;
			mipLevelSize.height /= 2;
		}

		//queue.release();
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
		textureDesc.dimension = TextureDimension::e2D;
		textureDesc.format = TextureFormat::RGBA8Unorm; // by convention for bmp, png and jpg file. Be careful with other formats.
		textureDesc.size = { (unsigned int)width, (unsigned int)height, 1 };
		textureDesc.mipLevelCount = bit_width(std::max(textureDesc.size.width, textureDesc.size.height));
		textureDesc.sampleCount = 1;
		textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
		textureDesc.viewFormatCount = 0;
		textureDesc.viewFormats = nullptr;
		Texture texture = Context::getInstance().getDevice().CreateTexture(&textureDesc);

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
			textureViewDesc.dimension = TextureViewDimension::e2D;
			textureViewDesc.format = textureDesc.format;
			*pTextureView = texture.CreateView(&textureViewDesc);
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
		textureDesc.dimension = wgpu::TextureDimension::e2D;
		textureDesc.mipLevelCount = 1;
		textureDesc.sampleCount = 1;

		// Create the texture
		Texture texture = Context::getInstance().getDevice().CreateTexture(&textureDesc);

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

		Context::getInstance().getDevice().GetQueue().WriteTexture(&imageCopyTexture, whitePixel, sizeof(whitePixel), &textureDataLayout, &textureSize);

		if (pTextureView) {
			TextureViewDescriptor textureViewDesc;
			textureViewDesc.aspect = TextureAspect::All;
			textureViewDesc.baseArrayLayer = 0;
			textureViewDesc.arrayLayerCount = 1;
			textureViewDesc.baseMipLevel = 0;
			textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
			textureViewDesc.dimension = TextureViewDimension::e2D;
			textureViewDesc.format = textureDesc.format;
			*pTextureView = texture.CreateView(&textureViewDesc);
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
		return Context::getInstance().getDevice().CreateSampler(&samplerDesc);
	}


	void loadMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh,entt::entity parent, Issam::Scene* scene)
	{
		int primitiveIdx = 0;
		for (const auto& primitive : mesh.primitives)
		{
			entt::entity entity = scene->addEntity();
			scene->addChild(parent, entity);
			std::vector<Vertex> vertices;
			std::vector<uint16_t> indices;
			// Process vertex attributes
			const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
			if (posAccessor.bufferView == -1) return ;
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
			MeshPtr myMesh = std::make_shared<Mesh>();
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
			

			Issam::MeshRenderer meshRenderer;
			meshRenderer.mesh = myMesh;
			if (primitive.material != -1)
			{
				Material* material = new Material();
				auto& gltfMaterial = model.materials[primitive.material];
				auto& baseColorFactor = gltfMaterial.pbrMetallicRoughness.baseColorFactor;
				material->setAttribute("baseColorFactor", glm::make_vec4(baseColorFactor.data()));

				int baseColorTextureIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
				if (baseColorTextureIndex >= 0 && baseColorTextureIndex < model.textures.size())
				{
					const tinygltf::Texture& gltfTexture = model.textures[baseColorTextureIndex];
					const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
					material->setAttribute("baseColorTexture", TextureManager::getInstance().getTextureView(gltfImage.uri));
				}

				int metallicRoughnessIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
				if (metallicRoughnessIndex >= 0 && metallicRoughnessIndex < model.textures.size())
				{
					const tinygltf::Texture& gltfTexture = model.textures[metallicRoughnessIndex];
					const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
					material->setAttribute("metallicRoughnessTexture", TextureManager::getInstance().getTextureView(gltfImage.uri));
				}

				auto& metallicFactor = gltfMaterial.pbrMetallicRoughness.metallicFactor;
				material->setAttribute("metallicFactor", (float) metallicFactor);

				auto& roughnessFactor = gltfMaterial.pbrMetallicRoughness.roughnessFactor;
				material->setAttribute("roughnessFactor", (float)roughnessFactor);

				material->setAttribute("unlitMaterialModel", "colorFactor", glm::vec4(1.0, 0.5, 0.0, 1.0), 0); 
				material->setAttribute("unlitMaterialModel", "colorFactor", glm::vec4(0.0), 1);

				meshRenderer.material = material;

				Issam::Filters filters;
				filters.add("pbr");
				scene->addComponent<Issam::Filters>(entity, filters);

			}

			scene->addComponent<Issam::MeshRenderer>(entity, meshRenderer);
		}
	}

	std::string GetBaseDir(const std::string& filepath) {
		if (filepath.find_last_of("/\\") != std::string::npos)
			return filepath.substr(0, filepath.find_last_of("/\\") + 1);
		return "";
	}

	void loadNode(const tinygltf::Model& model, const tinygltf::Node& gltfNode, entt::entity entity, Issam::Scene* scene, int idx) {
		static int nodeId = 0;
		std::string nodeName = gltfNode.name;
		if (nodeName.empty())
			nodeName = "node_" + std::to_string(idx);


		scene->addComponent<Issam::Name>(entity, Issam::Name({ nodeName }));

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

		//node->setTransform(glm::translate(glm::mat4(1.0), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0), S));
		scene->setLocalTransform(entity, glm::translate(glm::mat4(1.0), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0), S));

		if (!gltfNode.matrix.empty()) {
			scene->setLocalTransform(entity, glm::make_mat4(gltfNode.matrix.data()));
			//node->setTransform(glm::make_mat4(gltfNode.matrix.data()));
		}

		if (gltfNode.mesh >= 0) {
			loadMesh(model, model.meshes[gltfNode.mesh], entity, scene);
		}

		for (int childIndex : gltfNode.children) {
			entt::entity childEntity = scene->addEntity();
			loadNode(model, model.nodes[childIndex], childEntity, scene,  childIndex);
			scene->addChild(entity, childEntity);
			//node->children.push_back(childNode);
		}
	}



	const entt::entity& LoadGLTF(const std::string& filepath, Issam::Scene* scene) {
		entt::entity gltfEntity = scene->addEntity();
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
			return gltfEntity;
		}

		std::string baseDir = GetBaseDir(filepath);
		for (auto& gltfTexture : model.textures)
		{
			const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
			std::string texturePath = baseDir + "/" + gltfImage.uri;
			TextureView textureView = nullptr;
			Texture gltfTexture = Utils::loadTexture(texturePath, &textureView);
			TextureManager().getInstance().add(gltfImage.uri, textureView);
		}
		
		for (const auto& sceneNodeIndex : model.scenes[model.defaultScene].nodes) {
			entt::entity rootNode = scene->addEntity();
			scene->addChild(gltfEntity, rootNode);
			loadNode(model, model.nodes[sceneNodeIndex], rootNode, scene, sceneNodeIndex);
			//scene.push_back(rootNode);
		}
		
		//return scene;
		return gltfEntity;
	}
}