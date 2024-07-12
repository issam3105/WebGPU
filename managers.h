#pragma once


#include "context.h"
#include "mesh.h"
#include "shader.h"

class MeshManager
{
public:
	///
	static MeshManager& getInstance() {
		static MeshManager meshManager;
		return meshManager;
	};

	bool add(const std::string& id, Mesh* mesh) { m_meshes[id] = mesh; return true; }
	bool remove(const std::string& id) {
		m_meshes.erase(id); 
		return true; 
	}
	bool clear()
	{
	    m_meshes.clear();
		return true;
	}
	Mesh* get(const std::string& id) { return  m_meshes[id]; }
	std::unordered_map<std::string, Mesh*> getAll() { return m_meshes; }
private:
	std::unordered_map<std::string, Mesh*> m_meshes;
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

	bool add(const std::string& id, TextureView textureView) {
		m_textures[id] = std::make_shared<wgpu::TextureView>(textureView);
		return true; }
	TextureView getTextureView(const std::string& id) {
		return  *m_textures[id].get(); 
	}
	void clear()
	{
		m_textures.clear();
	}
	std::unordered_map<std::string, std::shared_ptr<TextureView>>& getAll() { return m_textures; }
private:
	std::unordered_map<std::string, std::shared_ptr<TextureView>> m_textures{};
};

class SamplerManager
{
public:
	SamplerManager() = default;
	~SamplerManager() = default;

	static SamplerManager& getInstance() {
		static SamplerManager samplerManager;
		return samplerManager;
	};

	bool add(const std::string& id, Sampler textureView) {
		m_samplers[id] = std::make_shared<wgpu::Sampler>(textureView);
		return true;
	}
	Sampler getSampler(const std::string& id) {
		return  *m_samplers[id].get();
	}
	void clear()
	{
		m_samplers.clear();
	}
	std::unordered_map<std::string, std::shared_ptr<Sampler>>& getAll() { return m_samplers; }
private:
	std::unordered_map<std::string, std::shared_ptr<Sampler>> m_samplers{};
};

