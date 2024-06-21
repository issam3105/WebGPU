#pragma once


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"
#include "uniformsBuffer.h"

using namespace glm;

namespace Issam {

	struct Attribute
	{
		std::string name;
		Value value;
		int handle = -1;
	};

	class Attributed {
	public:
		void addAttribute(std::string name, const Value& defaultValue) {
			Attribute uniform;
			uniform.name = name;
			uniform.value = defaultValue;
			if (std::holds_alternative< glm::vec4>(defaultValue) || std::holds_alternative< float>(defaultValue))
			{
				uniform.handle = m_uniformsBuffer.allocateVec4();
			}
			else if (std::holds_alternative< glm::mat4x4>(defaultValue))
			{
				uniform.handle = m_uniformsBuffer.allocateMat4();
			}
			else
				assert(false);

			m_attributes.push_back(uniform);
			dirtyBindGroup = true;
		};

		void addTexture(const std::string& name, TextureView defaultTextureView) { m_textures.push_back({ name, defaultTextureView }); }
		void addSampler(const std::string& name, Sampler defaultSampler) { m_samplers.push_back({ name, defaultSampler }); }

		Attribute& getAttribute(std::string name) {
			auto it = std::find_if(m_attributes.begin(), m_attributes.end(), [name](const Attribute& obj) {
				return obj.name == name;
			});
			assert(it != m_attributes.end());
			return *it;
		}

		void setAttribute(std::string name, const Value& value)
		{
			auto& attribute = getAttribute(name);
			attribute.value = value;
			m_uniformsBuffer.set(attribute.handle, value);
		}

		void setTexture(const std::string& name, TextureView textureView)
		{
			auto it = std::find_if(m_textures.begin(), m_textures.end(), [name](const std::pair<std::string, TextureView>& obj) {
				return obj.first == name;
			});
			assert(it != m_textures.end());
			it->second = textureView;
			dirtyBindGroup = true;
		}

		void setSampler(const std::string& name, Sampler sampler)
		{
			auto it = std::find_if(m_samplers.begin(), m_samplers.end(), [name](const std::pair<std::string, Sampler>& obj) {
				return obj.first == name;
			});
			assert(it != m_samplers.end());
			it->second = sampler;
			dirtyBindGroup = true;
		}

		std::vector<Attribute>& getAttributes() { return m_attributes; }

		BindGroup getBindGroup(BindGroupLayout bindGroupLayout) {
			if (!dirtyBindGroup)
				return bindGroup;
			// Bind Group
			int binding = 0;
			std::vector<BindGroupEntry> bindGroupEntries;
			BindGroupEntry uniformBinding{};
			uniformBinding.binding = binding++;
			uniformBinding.buffer = m_uniformsBuffer.getBuffer();;
			uniformBinding.size = sizeof(UniformsData);
			bindGroupEntries.push_back(uniformBinding);

			for (const auto& texture : m_textures)
			{
				BindGroupEntry textureBinding{};
				// The index of the binding (the entries in bindGroupDesc can be in any order)
				textureBinding.binding = binding++;
				// The buffer it is actually bound to
				textureBinding.textureView = texture.second;
				bindGroupEntries.push_back(textureBinding);
			}

			for (const auto& sampler : m_samplers)
			{
				BindGroupEntry samplerBinding{};
				// The index of the binding (the entries in bindGroupDesc can be in any order)
				samplerBinding.binding = binding++;
				// The buffer it is actually bound to
				samplerBinding.sampler = sampler.second;
				bindGroupEntries.push_back(samplerBinding);
			}


			BindGroupDescriptor bindGroupDesc;
			bindGroupDesc.label = "uniforms";
			bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
			bindGroupDesc.entries = bindGroupEntries.data();
			bindGroupDesc.layout = bindGroupLayout;
			bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
			dirtyBindGroup = false;
			return bindGroup;
		}

	protected:
		std::vector<Attribute> m_attributes{};
		std::vector<std::pair<std::string, TextureView> > m_textures{};
		std::vector<std::pair<std::string, Sampler>> m_samplers{};

		UniformsBuffer m_uniformsBuffer{};
		BindGroup bindGroup{ nullptr };
		bool dirtyBindGroup = true;
	};
}