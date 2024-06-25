#pragma once

#include <webgpu/webgpu.hpp>
using namespace wgpu;

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
using namespace glm;

#include "uniformsBuffer.h"
#include "attributed.h"
#include "managers.h"

const std::string c_pbrMaterialAttributes = "pbrMaterialModel";
const std::string c_unlitMaterialAttributes = "unlitMaterialModel";

//class MaterialModel : public Issam::Attributed {
//public:
//	MaterialModel()
//	{	
//	};
//	~MaterialModel() = default;
//};




class Material : public Issam::AttributedRuntime
{
public:
	Material(const std::string&  materialAttributesId)
	{
		setAttributes(materialAttributesId);
		//auto materialModel = Issam::AttributedManager::getInstance().get(materialAttributesId);
		//m_attributes = materialModel.getAttributes(); //Une copie
		//for (auto& attribute : m_attributes)
		//{
		//	if (std::holds_alternative< glm::vec4>(attribute.value) || std::holds_alternative< float>(attribute.value))
		//	{
		//		attribute.handle = m_uniformsBuffer.allocateVec4();
		//	}
		//	else if (std::holds_alternative< glm::mat4x4>(attribute.value))
		//	{
		//		attribute.handle = m_uniformsBuffer.allocateMat4();
		//	}
		//	else if (std::holds_alternative<TextureView>(attribute.value))
		//	{
		//		m_textures.push_back({ attribute.name, std::get< TextureView>(attribute.value) });
		//	}

		//	else if (std::holds_alternative<Sampler>(attribute.value))
		//	{
		//		m_samplers.push_back({ attribute.name, std::get< Sampler>(attribute.value) });
		//	}

		//	else
		//		assert(false);
		//}
		//m_textures = materialModel.getTextures();
	//	m_samplers = materialModel.getSamplers();
	};
	~Material() = default;

//	Issam::Attribute& getAttribute(std::string name) {
//		auto it = std::find_if(m_attributes.begin(), m_attributes.end(), [name](const Issam::Attribute& obj) {
//			return obj.name == name;
//		});
//		assert(it != m_attributes.end());
//		return *it;
//	}
//
//	void setAttribute(std::string name, const Value& value)
//	{
//		auto& attribute = getAttribute(name);
//		attribute.value = value;
//		if (std::holds_alternative< glm::vec4>(value) || std::holds_alternative< float>(value) || std::holds_alternative< glm::mat4x4>(value))
//		{
//			m_uniformsBuffer.set(attribute.handle, value);
//		}
//		else if (std::holds_alternative<TextureView>(value))
//		{
//			setTexture(name, std::get< TextureView>(value));
//		}
//		else if (std::holds_alternative<Sampler>(value))
//			setSampler(name, std::get< Sampler>(value));
//		else
//			assert(false);
//
//	}
//
//	BindGroup getBindGroup(BindGroupLayout bindGroupLayout) {
//		if (!dirtyBindGroup)
//			return bindGroup;
//		// Bind Group
//		int binding = 0;
//		std::vector<BindGroupEntry> bindGroupEntries;
//		BindGroupEntry uniformBinding{};
//		uniformBinding.binding = binding++;
//		uniformBinding.buffer = m_uniformsBuffer.getBuffer();;
//		uniformBinding.size = sizeof(UniformsData);
//		bindGroupEntries.push_back(uniformBinding);
//
//		for (const auto& texture : m_textures)
//		{
//			BindGroupEntry textureBinding{};
//			// The index of the binding (the entries in bindGroupDesc can be in any order)
//			textureBinding.binding = binding++;
//			// The buffer it is actually bound to
//			textureBinding.textureView = texture.second;
//			bindGroupEntries.push_back(textureBinding);
//		}
//
//		for (const auto& sampler : m_samplers)
//		{
//			BindGroupEntry samplerBinding{};
//			// The index of the binding (the entries in bindGroupDesc can be in any order)
//			samplerBinding.binding = binding++;
//			// The buffer it is actually bound to
//			samplerBinding.sampler = sampler.second;
//			bindGroupEntries.push_back(samplerBinding);
//		}
//
//
//		BindGroupDescriptor bindGroupDesc;
//		bindGroupDesc.label = "uniforms";
//		bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
//		bindGroupDesc.entries = bindGroupEntries.data();
//		bindGroupDesc.layout = bindGroupLayout;
//		bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
//		dirtyBindGroup = false;
//		return bindGroup;
//	}
//
//private:
//	std::vector<std::pair<std::string, TextureView> > m_textures{};
//	std::vector<std::pair<std::string, Sampler>> m_samplers{};
//	UniformsBuffer m_uniformsBuffer{};
//
//	void setTexture(const std::string& name, TextureView textureView)
//	{
//		auto it = std::find_if(m_textures.begin(), m_textures.end(), [name](const std::pair<std::string, TextureView>& obj) {
//			return obj.first == name;
//		});
//		assert(it != m_textures.end());
//		it->second = textureView;
//		dirtyBindGroup = true;
//	}
//
//	void setSampler(const std::string& name, Sampler sampler)
//	{
//		auto it = std::find_if(m_samplers.begin(), m_samplers.end(), [name](const std::pair<std::string, Sampler>& obj) {
//			return obj.first == name;
//		});
//		assert(it != m_samplers.end());
//		it->second = sampler;
//		dirtyBindGroup = true;
//	}
//
//	BindGroup bindGroup{ nullptr };
//	bool dirtyBindGroup = true;
//
//	std::vector<std::pair<std::string, TextureView> > m_textures{};
//	std::vector<std::pair<std::string, Sampler>> m_samplers{};
//	UniformsBuffer m_uniformsBuffer{};
//
//	std::vector<Issam::Attribute> m_attributes{};
};



