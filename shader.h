#pragma once

//#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"
//#include "material.h"
#include "uniformsBuffer.h"
#include "scene.h"
#include "material.h"


using namespace wgpu;
using namespace glm;


class Shader
{
public:

	Shader() {};

	~Shader() {
		m_shaderModule.release();
	}
	
	//enum class Binding : uint8_t
	//{
	//	Material = 0,
	//	Node,
	//	Scene
	//};

	void setUserCode(std::string userCode)
	{
		m_shaderSource = userCode;
		m_dirtyShaderModule = true;
	}


	struct VertexAttr
	{
		std::string name;
		int location;
		VertexFormat format;
	};

	void addVertexInput(const std::string& inputName, int location, VertexFormat format)
	{
		m_vertexInputs.push_back({ inputName , location,format });
		m_dirtyBindGroupLayouts = true;
		m_dirtyShaderModule = true;
	}

	void addVertexOutput(const std::string& inputName, int location, VertexFormat format)
	{
		m_vertexOutputs.push_back({ inputName , location,format });
		m_dirtyBindGroupLayouts = true;
		m_dirtyShaderModule = true;
	}

	

	ShaderModule getShaderModule() {
		if (!m_dirtyShaderModule)
			return m_shaderModule;

		//ShaderModule
		ShaderModuleDescriptor shaderDesc;
#ifdef WEBGPU_BACKEND_WGPU
		shaderDesc.hintCount = 0;
		shaderDesc.hints = nullptr;
#endif

		// Use the extension mechanism to load a WGSL shader source code
		ShaderModuleWGSLDescriptor shaderCodeDesc;
		// Set the chained struct's header
		shaderCodeDesc.chain.next = nullptr;
		shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
		// Connect the chain
		shaderDesc.nextInChain = &shaderCodeDesc.chain;

		std::string vertexInputOutputStr = "struct VertexInput {\n";
		for (const auto vertexInput : m_vertexInputs)
		{
			vertexInputOutputStr += "    @location(" + std::to_string(vertexInput.location) + ") " + vertexInput.name + ": " + toString(vertexInput.format) + ", \n";
		}
		vertexInputOutputStr += "}; \n \n";

		vertexInputOutputStr += "struct VertexOutput {\n";
		vertexInputOutputStr += "    @builtin(position) position: vec4f, \n";
		for (const auto vertexInput : m_vertexOutputs)
		{
			vertexInputOutputStr += "    @location(" + std::to_string(vertexInput.location) + ") " + vertexInput.name + ": " + toString(vertexInput.format) + ", \n";
		}
		vertexInputOutputStr += "}; \n \n";

		std::string vertexUniformsStr = "";
		int group = 0;
		addVertexUniformsStr(vertexUniformsStr, group, Issam::Binding::Material);
		addVertexUniformsStr(vertexUniformsStr, group, Issam::Binding::Node);
		addVertexUniformsStr(vertexUniformsStr, group, Issam::Binding::Scene);

		m_shaderSource = vertexInputOutputStr + vertexUniformsStr + m_shaderSource;

		// Setup the actual payload of the shader code descriptor
		shaderCodeDesc.code = m_shaderSource.c_str();
		std::cout << m_shaderSource << std::endl;

		m_shaderModule = Context::getInstance().getDevice().createShaderModule(shaderDesc);
		m_shaderModule.getCompilationInfo([](CompilationInfoRequestStatus status, const CompilationInfo& compilationInfo) {
			if (status == WGPUCompilationInfoRequestStatus_Success) {
				for (size_t i = 0; i < compilationInfo.messageCount; ++i) {
					const WGPUCompilationMessage& message = compilationInfo.messages[i];
					std::cerr << "Shader compilation message (" << message.type << "): " << message.message << std::endl;
					std::cerr << " - Line: " << message.lineNum << ", Column: " << message.linePos << std::endl;
					std::cerr << " - Offset: " << message.offset << ", Length: " << message.length << std::endl;
				}
			}
			else {
				std::cerr << "Failed to get shader compilation info: " << status << std::endl;
			}
		});
		assert(m_shaderModule);
		m_dirtyShaderModule = false;
		return m_shaderModule;
	}

	const std::vector<BindGroupLayout>& getBindGroupLayouts() {
		if (!m_dirtyBindGroupLayouts)
			return m_bindGroupLayouts;

		m_bindGroupLayouts.clear();
		addbindGroup(Issam::Binding::Material);
		addbindGroup(Issam::Binding::Node);
		addbindGroup(Issam::Binding::Scene);
		m_dirtyBindGroupLayouts = false;
		return m_bindGroupLayouts;
	}
	

	

	void addTexture(const std::string& name, TextureView defaultTextureView, Issam::Binding binding) { m_textures.push_back({ name, defaultTextureView, binding }); }
	void addSampler(const std::string& name, Sampler defaultSampler, Issam::Binding binding) { m_samplers.push_back({ name, defaultSampler, binding }); }
	void addUniform(std::string name, const UniformValue& defaultValue, Issam::Binding binding) {
		Uniform uniform;
		uniform.name = name;
		uniform.value = defaultValue;
		m_uniforms.push_back({ uniform, binding });
	};

	Uniform& getUniform(std::string name) {
		auto it = std::find_if(m_uniforms.begin(), m_uniforms.end(), [name](const std::pair<Uniform, Issam::Binding>& obj) {
			return obj.first.name == name;
		});
		assert(it != m_uniforms.end());
		return it->first;
	}

	bool hasUniform(std::string name) {
		auto it = std::find_if(m_uniforms.begin(), m_uniforms.end(), [name](const std::pair<Uniform, Issam::Binding>& obj) {
			return obj.first.name == name;
		});
		return (it != m_uniforms.end());
	}

	void setUniform(std::string name, const UniformValue& value, Issam::Binding binding)
	{
		auto& uniform = getUniform(name);
		uniform.value = value;
	}

	const std::string& getAttributedId(Issam::Binding binding)
	{
		return m_attributes[binding];
	}

	void addGroup(const std::string& groupId) { 
		
	//	m_material = material; 
		auto attributesGroup = Issam::AttributedManager::getInstance().get(groupId);
		auto attributes = attributesGroup.getAttributes();
		Issam::Binding binding = attributesGroup.getBinding();
		m_attributes[binding] = groupId;

		for (auto attrib : attributes)
		{
			if (std::holds_alternative< UniformValue>(attrib.value))
			{
				Uniform uniform;
				uniform.name = attrib.name;
				uniform.value = std::get <UniformValue>(attrib.value);
				m_uniforms.push_back({ uniform, binding });
			}
			else if (std::holds_alternative<TextureView>(attrib.value))
			{
				m_textures.push_back({ attrib.name, std::get<TextureView>(attrib.value), binding });
			}
			else if (std::holds_alternative<Sampler>(attrib.value))
				m_samplers.push_back({ attrib.name, std::get<Sampler>(attrib.value), binding });
			else
				assert(false);
		}
		
	}

private:
//	Material* m_material = nullptr;
	std::vector<std::pair<Uniform, Issam::Binding>> m_uniforms{};
	std::vector<std::tuple<std::string, TextureView, Issam::Binding> > m_textures{};
	std::vector<std::tuple<std::string, Sampler, Issam::Binding>> m_samplers{};
	std::vector<BindGroupLayout> m_bindGroupLayouts{};
	bool m_dirtyBindGroupLayouts = true;

	std::unordered_map<Issam::Binding, std::string> m_attributes{};

	std::vector<Uniform> getUniformsByBinding(Issam::Binding binding) {
		std::vector<Uniform> result;
		for (const auto& [uniform, bind] : m_uniforms) {
			if (bind == binding) {
				result.push_back(uniform);
			}
		}
		return result;
	}

	std::vector<std::pair<std::string, TextureView>> getTexturesByBinding(Issam::Binding binding) {
		std::vector<std::pair<std::string, TextureView>> result;
		for (const auto& [name, view , bind] : m_textures) {
			if (bind == binding) {
				result.push_back({ name, view });
			}
		}
		return result;
	}

	std::vector<std::pair<std::string, Sampler>> getSamplersByBinding(Issam::Binding binding) {
		std::vector<std::pair<std::string, Sampler>> result;
		for (const auto& [name, sampler, bind] : m_samplers) {
			if (bind == binding) {
				result.push_back({ name, sampler });
			}
		}
		return result;
	}

	std::string toString(VertexFormat format)
	{
		switch (format)
		{
		case VertexFormat::Float32x2: return "vec2f";
		case VertexFormat::Float32x3: return "vec3f";
		case VertexFormat::Float32x4: return "vec4f";
		default:
			assert(false);
			return "UNKNOWN";
			break;
		}
	}

	std::string toString(Issam::Binding binding)
	{
		switch (binding)
		{
		case Issam::Binding::Material: return "Material";
		case Issam::Binding::Node:     return "Node";
		case Issam::Binding::Scene:    return "Scene";
		default: assert(false);
		}
	}

	std::string toLowerCase(const std::string& str) {
		std::string result = str;
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
			return std::tolower(c);
		});
		return result;
	}

	void addVertexUniformsStr(std::string& vertexUniformsStr, int& group, Issam::Binding binding)
	{
		int bindingIdx = 0;
		bool usedGroupe = false;
		const std::vector<Uniform>& materialUniforms = getUniformsByBinding(binding);
		if (!materialUniforms.empty())
		{
			usedGroupe = true;
			vertexUniformsStr += "struct " + toString(binding) + " { \n";
			for (const auto& uniform : materialUniforms)
			{
				vertexUniformsStr += "    " + uniform.name + ": " + (uniform.isMatrix() ? "mat4x4f" : "vec4f") + ", \n";
			}
			vertexUniformsStr += "}; \n \n";
			vertexUniformsStr += "@group(" + std::to_string(group) + ") @binding(" + std::to_string(bindingIdx++) + ") var<uniform> u_" + toLowerCase(toString(binding)) + ": " + toString(binding) + ";\n";
		}

		const auto& materialTextures = getTexturesByBinding(binding);
		if (!materialTextures.empty())
		{
			usedGroupe = true;
			for (const auto& texture : materialTextures)
			{
				vertexUniformsStr += "@group(" + std::to_string(group) + ") @binding(" + std::to_string(bindingIdx++) + ") var " + std::get<0>(texture) + " : texture_2d<f32>;\n";
			}
		}

		const auto& materialSamplers = getSamplersByBinding(binding);
		if (!materialSamplers.empty())
		{
			usedGroupe = true;
			for (const auto& sampler : m_samplers)
			{
				vertexUniformsStr += "@group(" + std::to_string(group) + ") @binding(" + std::to_string(bindingIdx++) + ") var " + std::get<0>(sampler) + " : sampler;\n";
			}
			vertexUniformsStr += "\n \n";
		}
		if (usedGroupe) group++;
	}

	void addbindGroup(Issam::Binding binding)
	{
		const std::string groupId = getAttributedId(binding);
		auto& attributesGroup = Issam::AttributedManager::getInstance().get(groupId);
		const std::vector<Uniform>& materialUniforms = getUniformsByBinding(binding);
		int bindingIdx = 0;
		bool usedGroupe = false;
		std::vector<BindGroupLayoutEntry> bindingLayoutEntries{};
		if (!materialUniforms.empty())
		{
			usedGroupe = true;
			BindGroupLayoutEntry uniformsBindingLayout = Default;
			// The binding index as used in the @binding attribute in the shader
			uniformsBindingLayout.binding = bindingIdx++;
			// The stage that needs to access this resource
			uniformsBindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
			uniformsBindingLayout.buffer.type = BufferBindingType::Uniform;
			uniformsBindingLayout.buffer.minBindingSize = sizeof(UniformsData);
			uniformsBindingLayout.buffer.hasDynamicOffset = attributesGroup.getVersionCount() > 1 ? true : false;
			bindingLayoutEntries.push_back(uniformsBindingLayout);
		}

		const auto& materialTextures = getTexturesByBinding(binding);
		if (!materialTextures.empty())
		{
			usedGroupe = true;
			for (const auto& texture : materialTextures)
			{
				// The texture binding
				BindGroupLayoutEntry textureBindingLayout = Default;
				textureBindingLayout.binding = bindingIdx++;
				textureBindingLayout.visibility = ShaderStage::Fragment;
				textureBindingLayout.texture.sampleType = TextureSampleType::Float;
				textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;
				bindingLayoutEntries.push_back(textureBindingLayout);
			}
		}

		const auto& materialSamplers = getSamplersByBinding(binding);
		if (!materialSamplers.empty())
		{
			usedGroupe = true;
			for (const auto& sampler : m_samplers)
			{
				// The texture sampler binding
				BindGroupLayoutEntry samplerBindingLayout = Default;
				samplerBindingLayout.binding = bindingIdx++;
				samplerBindingLayout.visibility = ShaderStage::Fragment;
				samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;
				bindingLayoutEntries.push_back(samplerBindingLayout);
			}
		}

		if (usedGroupe)
		{
			// Create a bind group layout
			BindGroupLayoutDescriptor bindGroupLayoutDesc{};
			bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();;
			bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
			m_bindGroupLayouts.push_back(Context::getInstance().getDevice().createBindGroupLayout(bindGroupLayoutDesc));
		}
	}

	std::vector<VertexAttr> m_vertexInputs{};
	std::vector<VertexAttr> m_vertexOutputs{};
	ShaderModule m_shaderModule{ nullptr };
	bool m_dirtyShaderModule = true;

//	UniformsBuffer m_uniformsBuffer[3]{};
	BindGroup sceneBindGroup{ nullptr };
	bool sceneDirtyBindGroup = true;
	
	std::string m_shaderSource;
};
