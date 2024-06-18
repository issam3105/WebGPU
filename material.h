#pragma once

#include <webgpu/webgpu.hpp>
using namespace wgpu;

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
using namespace glm;

#include "uniformsBuffer.h"

#include <array>

struct Uniform {
	std::string name;
	int handle;
	Value value;
	bool isMatrix() const { return std::holds_alternative< glm::mat4>(value); }
};

class Material {
public:
	Material() = default;
	~Material() = default;
	void addTexture(const std::string& name, TextureView defaultTextureView) { m_textures.push_back({ name, defaultTextureView }); }

	void setTexture(const std::string& name, TextureView textureView)
	{
		auto it = std::find_if(m_textures.begin(), m_textures.end(), [name](const std::pair<std::string, TextureView>& obj) {
			return obj.first == name;
		});
		assert(it != m_textures.end());
		it->second = textureView;
		m_dirtyBindGroup = true;
	}


	void addSampler(const std::string& name, Sampler defaultSampler) { m_samplers.push_back({ name, defaultSampler }); }

	void setSampler(const std::string& name, Sampler sampler)
	{
		auto it = std::find_if(m_samplers.begin(), m_samplers.end(), [name](const std::pair<std::string, Sampler>& obj) {
			return obj.first == name;
		});
		assert(it != m_samplers.end());
		it->second = sampler;
		m_dirtyBindGroup = true;
	}


	void addUniform(std::string name, const Value& defaultValue) {
		uint16_t handle = -1;
		if (std::holds_alternative< glm::vec4>(defaultValue) || std::holds_alternative< float>(defaultValue))
			handle = m_uniformsBuffer.allocateVec4();
		else if (std::holds_alternative< glm::mat4x4>(defaultValue))
			handle = m_uniformsBuffer.allocateMat4();
		else
			assert(false);
		m_uniformsBuffer.set(handle, defaultValue);
		m_uniforms.push_back({ name, handle, defaultValue });
	};


	Uniform& getUniform(std::string name) {
		auto it = std::find_if(m_uniforms.begin(), m_uniforms.end(), [name](const Uniform& obj) {
			return obj.name == name;
		});
		assert(it != m_uniforms.end());
		return *it;
	}


	void setUniform(std::string name, const Value& value)
	{
		auto& uniform = getUniform(name);
		uniform.value = value;
		m_uniformsBuffer.set(uniform.handle, value);
		m_dirtyBindGroup = true;
	}

	BindGroup getBindGroup(BindGroupLayout bindGroupLayout) {
		if (m_dirtyBindGroup)
			refreshBinding(bindGroupLayout);
		return m_bindGroup;
	}

	std::vector<Uniform>& getUniforms() { return m_uniforms; }
	std::vector<std::pair<std::string, TextureView> >& getTextures() { return m_textures; }
	std::vector<std::pair<std::string, Sampler>>& getSamplers() { return m_samplers; }

private:

	void refreshBinding(BindGroupLayout bindGroupLayout)
	{
		//To CALL AFTER build()
		int binding = 0;
		std::vector<BindGroupEntry> bindings;
		{
			// Create a binding
			BindGroupEntry uniformBinding{};
			// The index of the binding (the entries in bindGroupDesc can be in any order)
			uniformBinding.binding = binding++;
			// The buffer it is actually bound to
			uniformBinding.buffer = m_uniformsBuffer.getBuffer();
			// We can specify an offset within the buffer, so that a single buffer can hold
			// multiple uniform blocks.
			uniformBinding.offset = 0;
			// And we specify again the size of the buffer.
			uniformBinding.size = sizeof(std::array<vec4, UNIFORMS_MAX>);
			bindings.push_back(uniformBinding);
		}

		for (const auto& texture : m_textures)
		{
			BindGroupEntry textureBinding{};
			// The index of the binding (the entries in bindGroupDesc can be in any order)
			textureBinding.binding = binding++;
			// The buffer it is actually bound to
			textureBinding.textureView = texture.second;
			bindings.push_back(textureBinding);
		}

		for (const auto& sampler : m_samplers)
		{
			BindGroupEntry samplerBinding{};
			// The index of the binding (the entries in bindGroupDesc can be in any order)
			samplerBinding.binding = binding++;
			// The buffer it is actually bound to
			samplerBinding.sampler = sampler.second;
			bindings.push_back(samplerBinding);
		}

		// A bind group contains one or multiple bindings
		BindGroupDescriptor bindGroupDesc;
		bindGroupDesc.label = "bindGroup 0 : global uniforms";
		bindGroupDesc.layout = bindGroupLayout;
		// There must be as many bindings as declared in the layout!
		bindGroupDesc.entryCount = (uint32_t)bindings.size();
		bindGroupDesc.entries = bindings.data();
		m_bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
		m_dirtyBindGroup = false;
	}

	bool m_dirtyBindGroup = true;

	BindGroup m_bindGroup{ nullptr };

	std::vector<Uniform> m_uniforms{};
	std::vector<std::pair<std::string, TextureView> > m_textures{};
	std::vector<std::pair<std::string, Sampler>> m_samplers{};
	UniformsBuffer m_uniformsBuffer{};

};