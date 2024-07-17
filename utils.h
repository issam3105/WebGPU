#pragma once

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

#include "context.h"
#include "scene.h"
#include "material.h"

#include "stb_image.h"

namespace fs = std::filesystem;
namespace Utils
{
	entt::entity createBoundingBox(Issam::Scene* scene, const vec3& point1, const vec3& point2);

	entt::entity addLine(Issam::Scene* scene, glm::vec3 start, glm::vec3 end, glm::vec4 color);

	entt::entity addAxes(Issam::Scene* scene);

	wgpu::Texture CreateWhiteTexture(TextureView* pTextureView);

	std::string loadFile(const std::filesystem::path& path);

	Sampler createDefaultSampler();

	std::string GetBaseDir(const std::string& filepath);

	std::string getFileExtension(const std::string& filePath);
	
	Texture loadTexture(unsigned char* pixelData, int& width, int& height, int& channels, TextureView* pTextureView = nullptr);

	Texture loadImageFromPath(const std::string& path, TextureView* pTextureView = nullptr);
}
