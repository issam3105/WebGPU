#pragma once


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"
#include "uniformsBuffer.h"

using namespace glm;

namespace Issam {

	using AttributeValue = std::variant<UniformValue, TextureView, Sampler>;

	struct Attribute
	{
		std::string name;
		AttributeValue value;
		int handle = -1;
	};

	enum class Binding : uint8_t
	{
		Material = 0,
		Node,
		Scene
	};


	class AttributeGroup {
	public:
		AttributeGroup(Binding binding = Binding::Material, int versionCount = 1) :
			m_binding(binding) ,
			m_versionCount(versionCount)
		{};
		void addAttribute(std::string name, const AttributeValue& defaultValue) {
			Attribute attribute;
			attribute.name = name;
			attribute.value = defaultValue;
			m_attributes.push_back(attribute);
		};

		std::vector<Attribute>& getAttributes() { return m_attributes; }	
		const Binding& getBinding()const { return m_binding; }
		const int getVersionCount()const { return m_versionCount; }
	protected:
		std::vector<Attribute> m_attributes{};
		Binding m_binding{ Binding::Material };
		int m_versionCount = 1;
	};

	class AttributedManager
	{
	public:
		AttributedManager() = default;
		~AttributedManager() = default;

		static AttributedManager& getInstance() {
			static AttributedManager attributedManager;
			return attributedManager;
		};

		bool add(const std::string& id, Issam::AttributeGroup attributeGroup) {
			m_attributeGroups[id] = attributeGroup;
			return true;
		}
		const Issam::AttributeGroup& get(const std::string& id) {
			return  m_attributeGroups[id];
		}
		void clear()
		{
			m_attributeGroups.clear();
		}
		std::unordered_map<std::string, Issam::AttributeGroup>& getAll() { return m_attributeGroups; }
		std::unordered_map<std::string, Issam::AttributeGroup> getAll(Binding binding) {
			std::unordered_map<std::string, Issam::AttributeGroup> list;
			for (auto& group : m_attributeGroups)
			{
				if (group.second.getBinding() == binding)
					list[group.first] = group.second;
			}
			return list;
		}
	private:
		std::unordered_map<std::string, Issam::AttributeGroup> m_attributeGroups{};
	};

	class AttributedRuntime {
	public:
		AttributedRuntime() = delete;

		AttributedRuntime(const std::string& attributesId, size_t numVersions) {
			m_uniformsBuffer = UniformsBuffer(numVersions);
			m_numVersions = numVersions;
			setAttributes(attributesId); 
		};

		void setAttributes(const std::string& attributesId)
		{
			auto materialModel = Issam::AttributedManager::getInstance().get(attributesId);
			m_attributes = materialModel.getAttributes(); //Une copie
			for (auto& attribute : m_attributes)
			{
				if (std::holds_alternative< UniformValue>(attribute.value))
				{
					const UniformValue& uniformValue = std::get< UniformValue>(attribute.value);
					if (std::holds_alternative< glm::vec4>(uniformValue) || std::holds_alternative< float>(uniformValue))
					{
						attribute.handle = m_uniformsBuffer.allocate<glm::vec4>();
						setAttribute(attribute.name, attribute.value); //Apply default value TODO Tout les versions
					}
					else if (std::holds_alternative< glm::mat4x4>(uniformValue))
					{
						attribute.handle = m_uniformsBuffer.allocate<glm::mat4>();
						setAttribute(attribute.name, attribute.value); //Apply default value
					}
				}
				
				else if (std::holds_alternative<TextureView>(attribute.value))
				{
					m_textures.push_back({ attribute.name, std::get< TextureView>(attribute.value) });
				}

				else if (std::holds_alternative<Sampler>(attribute.value))
				{
					m_samplers.push_back({ attribute.name, std::get< Sampler>(attribute.value) });
				}

				else
					assert(false);
			}
		}

		Issam::Attribute& getAttribute(std::string name) {
			auto it = std::find_if(m_attributes.begin(), m_attributes.end(), [name](const Issam::Attribute& obj) {
				return obj.name == name;
			});
			assert(it != m_attributes.end());
			return *it;
		}

		bool hasAttribute(std::string name) {
			auto it = std::find_if(m_attributes.begin(), m_attributes.end(), [name](const Issam::Attribute& obj) {
				return obj.name == name;
			});
			return (it != m_attributes.end());
		}

		void setAttribute(std::string name, const AttributeValue& value, size_t version = 0)
		{
			auto& attribute = getAttribute(name);
			attribute.value = value;
			if (std::holds_alternative< UniformValue>(value))
			{
				m_uniformsBuffer.set(attribute.handle, std::get <UniformValue >(value), version);
			}
			else if (std::holds_alternative<TextureView>(value))
			{
				setTexture(name, std::get< TextureView>(value));
			}
			else if (std::holds_alternative<Sampler>(value))
				setSampler(name, std::get< Sampler>(value));
			else
				assert(false);

		}

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
		
		size_t getNumVersions() { return m_numVersions; }
	private:
		std::vector<std::pair<std::string, TextureView> > m_textures{};
		std::vector<std::pair<std::string, Sampler>> m_samplers{};
		UniformsBuffer m_uniformsBuffer;

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

		BindGroup bindGroup{ nullptr };
		bool dirtyBindGroup = true;

		size_t m_numVersions = 1;

		/*std::vector<std::pair<std::string, TextureView> > m_textures{};
		std::vector<std::pair<std::string, Sampler>> m_samplers{};
		UniformsBuffer m_uniformsBuffer{};*/
	protected:
		std::vector<Issam::Attribute> m_attributes{};
	};

	
}