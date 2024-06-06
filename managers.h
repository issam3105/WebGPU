#pragma once

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"

class MeshManager
{
public:
	///
	static MeshManager& getInstance() {
		static MeshManager meshManager;
		return meshManager;
	};

	bool add(const std::string& id, Mesh* mesh) { m_meshes[id] = mesh; return true; }
	Mesh* get(const std::string& id) { return  m_meshes[id]; }
	std::unordered_map<std::string, Mesh*> getAll() { return m_meshes; }
private:
	std::unordered_map<std::string, Mesh*> m_meshes;
};

class ShaderManager
{
public:
	ShaderManager() = default;
	~ShaderManager() = default;

	static ShaderManager& getInstance() {
		static ShaderManager shaderManager;
		return shaderManager;
	};

	bool add(const std::string& id, Shader* shader) {
		m_shaders[id] = shader;
		shader->build();
		return true;
	}
	Shader* getShader(const std::string& id) { return  m_shaders[id]; }
private:
	std::unordered_map<std::string, Shader*> m_shaders{};
};

class TextureManager
{
public:
	TextureManager() = default;
	~TextureManager() = default;

	static TextureManager& getInstance() {
		static TextureManager textureManager;
		return textureManager;
	};

	bool add(const std::string& id, wgpu::TextureView* textureView) { m_textures[id] = textureView; return true; }
	wgpu::TextureView* getTextureView(const std::string& id) { return  m_textures[id]; }
	std::unordered_map<std::string, wgpu::TextureView*>& getAll() { return m_textures; }
private:
	std::unordered_map<std::string, wgpu::TextureView*> m_textures{};
};