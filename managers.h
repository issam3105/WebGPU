#pragma once


#include "context.h"
#include "mesh.h"
#include "shader.h"





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
		m_textures[id] = textureView;
		return true; }
	TextureView getTextureView(const std::string& id) {
		return  m_textures[id]; 
	}

	bool remove(const std::string& id)
	{
		auto it =m_textures.find(id);
		if (it != m_textures.end())
		{
			m_textures.erase(it);
			return true;
		}
		return false;
	}
	void clear()
	{
		m_textures.clear();
	}
	std::unordered_map<std::string, TextureView>& getAll() { return m_textures; }
private:
	std::unordered_map<std::string, TextureView> m_textures{};
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

	bool add(const std::string& id, Sampler sampler) {
		m_samplers[id] = sampler;
		return true;
	}
	Sampler getSampler(const std::string& id) {
		return  m_samplers[id];
	}
	void clear()
	{
		m_samplers.clear();
	}
	std::unordered_map<std::string, Sampler>& getAll() { return m_samplers; }
private:
	std::unordered_map<std::string, Sampler> m_samplers{};
};

