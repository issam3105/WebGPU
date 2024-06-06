#pragma once

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"

namespace fs = std::filesystem;
namespace Utils
{
	void addTwoTriangles()
	{
		std::vector<float> positionData = {
		-0.5, -0.5,
		+0.5, -0.5,
		+0.0, +0.5,
		-0.55f, -0.5,
		-0.05f, +0.5,
		-0.55f, +0.5
		};

		// r0,  g0,  b0, r1,  g1,  b1, ...
		std::vector<float> colorData = {
			1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			0.0, 0.0, 1.0,
			1.0, 1.0, 0.0,
			1.0, 0.0, 1.0,
			0.0, 1.0, 1.0
		};

		int vertexCount = static_cast<int>(positionData.size() / 2);
		VertexBuffer* vertexBufferPos = new VertexBuffer(positionData.data(), positionData.size() * sizeof(float), vertexCount);
		vertexBufferPos->addVertexAttrib("Position", 0, VertexFormat::Float32x2, 0);
		vertexBufferPos->setAttribsStride(2 * sizeof(float));

		VertexBuffer* vertexBufferColor = new VertexBuffer(colorData.data(), colorData.size() * sizeof(float), vertexCount);
		vertexBufferColor->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 0); // offset = sizeof(Position)
		vertexBufferColor->setAttribsStride(3 * sizeof(float));

		Mesh* mesh = new Mesh();
		mesh->addVertexBuffer(vertexBufferPos);
		mesh->addVertexBuffer(vertexBufferColor);

		//interleaved  mesh
		//std::vector<float> vertexData = {
		//	// x0,  y0,  r0,  g0,  b0
		//	-0.5, -0.5, 1.0, 0.0, 0.0,

		//	// x1,  y1,  r1,  g1,  b1
		//	+0.5, -0.5, 0.0, 1.0, 0.0,

		//	// ...
		//	+0.0,   +0.5, 0.0, 0.0, 1.0,
		//	-0.55f, -0.5, 1.0, 1.0, 0.0,
		//	-0.05f, +0.5, 1.0, 0.0, 1.0,
		//	-0.55f, +0.5, 0.0, 1.0, 1.0
		//};
		//int vertexCount = static_cast<int>(vertexData.size() / 5);

		//VertexBuffer* vertexBuffer = new VertexBuffer(vertexData, vertexCount); // vertexData.size()* sizeof(float)
		//vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x2, 0);
		//vertexBuffer->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 2 * sizeof(float)); // offset = sizeof(Position)
		//vertexBuffer->setAttribsStride(5 * sizeof(float));
		//Mesh* mesh = new Mesh();
		//mesh->addVertexBuffer(vertexBuffer);


		MeshManager::getInstance().add("twoTriangles", mesh);
	}


	void addColoredPlane() {
		std::vector<float> pointData = {
			// x,   y,     r,   g,   b
			-0.5, -0.5,   1.0, 0.0, 0.0,
			+0.5, -0.5,   0.0, 1.0, 0.0,
			+0.5, +0.5,   0.0, 0.0, 1.0,
			-0.5, +0.5,   1.0, 1.0, 0.0
		};

		std::vector<uint16_t> indexData = {
		0, 1, 2, // Triangle #0
		0, 2, 3  // Triangle #1
		};

		int indexCount = static_cast<int>(indexData.size());
		int vertexCount = static_cast<int>(pointData.size() / 5);

		VertexBuffer* vertexBuffer = new VertexBuffer(pointData.data(), pointData.size() * sizeof(float), vertexCount);
		vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x2, 0);
		vertexBuffer->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 2 * sizeof(float)); // offset = sizeof(Position)
		vertexBuffer->setAttribsStride(5 * sizeof(float));

		IndexBuffer* indexBuffer = new IndexBuffer(indexData.data(), indexData.size(), indexCount);
		Mesh* mesh = new Mesh();
		mesh->addVertexBuffer(vertexBuffer);
		mesh->addIndexBuffer(indexBuffer);
		MeshManager::getInstance().add("ColoredPlane", mesh);
	}

	void addPyramid()
	{
		std::vector<float> pointData = {
			-0.5f, -0.5f, -0.3f,    1.0f, 1.0f, 1.0f,
			+0.5f, -0.5f, -0.3f,    1.0f, 1.0f, 1.0f,
			+0.5f, +0.5f, -0.3f,    1.0f, 1.0f, 1.0f,
			-0.5f, +0.5f, -0.3f,    1.0f, 1.0f, 1.0f,
			+0.0f, +0.0f, +0.5f,    0.5f, 0.5f, 0.5f,
		};
		std::vector<uint16_t> indexData = {
			0,  1,  2,
			0,  2,  3,
			0,  1,  4,
			1,  2,  4,
			2,  3,  4,
			3,  0,  4,
		};

		int indexCount = static_cast<int>(indexData.size());
		int vertexCount = static_cast<int>(pointData.size() / 6);

		VertexBuffer* vertexBuffer = new VertexBuffer(pointData.data(), pointData.size() * sizeof(float), vertexCount);
		vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x3, 0);
		vertexBuffer->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 3 * sizeof(float)); // offset = sizeof(Position)
		vertexBuffer->setAttribsStride(6 * sizeof(float));

		IndexBuffer* indexBuffer = new IndexBuffer(indexData.data(), indexData.size(), indexCount);
		Mesh* mesh = new Mesh();
		mesh->addVertexBuffer(vertexBuffer);
		mesh->addIndexBuffer(indexBuffer);
		MeshManager::getInstance().add("Pyramid", mesh);
	}

	struct VertexAttributes {
		vec3 position;
		vec3 normal;
		vec3 color;
		vec2 uv;
	};

	bool loadGeometryFromObj(const fs::path& path) {
		std::vector<VertexAttributes> vertexData;
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
		vertexData.clear();
		for (const auto& shape : shapes) {
			size_t offset = vertexData.size();
			vertexData.resize(offset + shape.mesh.indices.size());

			for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
				const tinyobj::index_t& idx = shape.mesh.indices[i];

				vertexData[offset + i].position = {
					attrib.vertices[3 * idx.vertex_index + 0],
					-attrib.vertices[3 * idx.vertex_index + 2], // Add a minus to avoid mirroring
					attrib.vertices[3 * idx.vertex_index + 1]
				};

				// Also apply the transform to normals!!
				vertexData[offset + i].normal = {
					attrib.normals[3 * idx.normal_index + 0],
					-attrib.normals[3 * idx.normal_index + 2],
					attrib.normals[3 * idx.normal_index + 1]
				};

				vertexData[offset + i].color = {
					attrib.colors[3 * idx.vertex_index + 0],
					attrib.colors[3 * idx.vertex_index + 1],
					attrib.colors[3 * idx.vertex_index + 2]
				};

				vertexData[offset + i].uv = {
					attrib.texcoords[2 * idx.texcoord_index + 0],
					1 - attrib.texcoords[2 * idx.texcoord_index + 1]
				};
			}
		}

		int indexCount = static_cast<int>(vertexData.size());
		int vertexCount = static_cast<int>(vertexData.size());
		auto* data = vertexData.data();
		VertexBuffer* vertexBuffer = new VertexBuffer(vertexData.data(), vertexData.size() * sizeof(VertexAttributes), vertexCount);
		vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x3, 0);
		vertexBuffer->addVertexAttrib("Normal", 1, VertexFormat::Float32x3, 3 * sizeof(float));
		vertexBuffer->addVertexAttrib("Color", 2, VertexFormat::Float32x3, 6 * sizeof(float));
		vertexBuffer->addVertexAttrib("TexCoord", 3, VertexFormat::Float32x2, 9 * sizeof(float));
		vertexBuffer->setAttribsStride(11 * sizeof(float));


		Mesh* mesh = new Mesh();
		mesh->addVertexBuffer(vertexBuffer);
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

	Texture loadTexture(const fs::path& path, TextureView* pTextureView) {
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
}