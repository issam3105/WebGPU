#pragma once

#include <iostream>
#include <cassert>
#include <fstream>
#include <regex>

#include "scene.h"

#include "tiny_gltf.h"

#include <entt/entt.hpp>

#include "utils.h"


const std::regex base64Pattern(R"(data:image/(\w+);base64,)");

class GltfLoader
{
public:
	GltfLoader() = delete;
	GltfLoader(Issam::Scene* scene) : m_scene(scene) {};
	~GltfLoader() = default;

	void load(const std::string& filepath);
	void unload();

private:
	Texture loadImageFromBase64(const std::string& base64, TextureView* pTextureView = nullptr);
	Texture loadImageFromVector(const std::vector<unsigned char>& imageVector, TextureView* pTextureView = nullptr);
	Texture loadImageFromBuffer(const tinygltf::Model& model, const tinygltf::Image& image, TextureView* pTextureView = nullptr);

	std::string generateTextureId(const std::string& uri, const std::string& baseDir, int source);

	void loadMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, entt::entity parent, Issam::Scene* scene);
	void loadNode(const tinygltf::Model& model, const tinygltf::Node& gltfNode, entt::entity entity, Issam::Scene* scene, int idx);

private:
	Issam::Scene* m_scene;
	entt::entity m_entity;
	std::unordered_map<int, std::string> m_sourceToId;
};
