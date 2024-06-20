#pragma once

//#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"
#include "material.h"
#include "uniformsBuffer.h"
#include "node.h"


using namespace wgpu;
using namespace glm;


class Shader
{
public:
	Shader() {};

	~Shader() {
		m_shaderModule.release();
	}

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

		std::string vertexUniformsStr = "struct Uniforms { \n";
		for (const auto& uniform : m_materialModule->getUniforms())
		{
			vertexUniformsStr += "    " + uniform.name + ": " + (uniform.isMatrix() ? "mat4x4f" : "vec4f") + ", \n";
		}
		vertexUniformsStr += "}; \n \n";
		//Binding groups
		{
			vertexUniformsStr += "@group(0) @binding(0) var<uniform> u_uniforms: Uniforms;\n";
			int binding = 1; //0 used for uniforms
			for (const auto& texture : m_materialModule->getTextures())
			{
				vertexUniformsStr += "@group(0) @binding(" + std::to_string(binding++) + ") var " + texture.first + " : texture_2d<f32>;\n";
			}
			for (const auto& sampler : m_materialModule->getSamplers())
			{
				vertexUniformsStr += "@group(0) @binding(" + std::to_string(binding++) + ") var " + sampler.first + " : sampler;\n";
			}
			vertexUniformsStr += "\n \n";
		}

		vertexUniformsStr += "@group(1) @binding(0) var<uniform> u_model: mat4x4f;\n \n";


		if (m_scene)
		{
			std::string sceneStructStr = "struct Scene { \n";
			for (const auto& uniform : m_scene->getUniforms())
			{
				sceneStructStr += "    " + uniform.name + ": " + (uniform.isMatrix() ? "mat4x4f" : "vec4f") + ", \n";
			}
			sceneStructStr += "}; \n \n";
			vertexUniformsStr += sceneStructStr;
			vertexUniformsStr += "@group(2) @binding(0) var<uniform> u_scene: Scene;\n";
		}

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


	const std::vector<BindGroupLayout>& getBindGroupLayouts()  { 
		if (!m_dirtyBindGroupLayouts)
			return m_bindGroupLayouts;

		{
			int binding = 0;
			std::vector<BindGroupLayoutEntry> bindingLayoutEntries{};

			BindGroupLayoutEntry uniformsBindingLayout = Default;
			// The binding index as used in the @binding attribute in the shader
			uniformsBindingLayout.binding = binding++;
			// The stage that needs to access this resource
			uniformsBindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
			uniformsBindingLayout.buffer.type = BufferBindingType::Uniform;
			uniformsBindingLayout.buffer.minBindingSize = sizeof(UniformsData);
			bindingLayoutEntries.push_back(uniformsBindingLayout);

			for (const auto& texture : m_materialModule->getTextures())
			{
				// The texture binding
				BindGroupLayoutEntry textureBindingLayout = Default;
				textureBindingLayout.binding = binding++;
				textureBindingLayout.visibility = ShaderStage::Fragment;
				textureBindingLayout.texture.sampleType = TextureSampleType::Float;
				textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;
				bindingLayoutEntries.push_back(textureBindingLayout);
			}

			for (const auto& sampler : m_materialModule->getSamplers())
			{
				// The texture sampler binding
				BindGroupLayoutEntry samplerBindingLayout = Default;
				samplerBindingLayout.binding = binding++;
				samplerBindingLayout.visibility = ShaderStage::Fragment;
				samplerBindingLayout.sampler.type = SamplerBindingType::Filtering;
				bindingLayoutEntries.push_back(samplerBindingLayout);
			}


			// Create a bind group layout
			BindGroupLayoutDescriptor bindGroupLayoutDesc{};
			bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();;
			bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
			m_bindGroupLayouts.push_back(Context::getInstance().getDevice().createBindGroupLayout(bindGroupLayoutDesc));
		}

		//Model matrix Uniform
		{
			std::vector<BindGroupLayoutEntry> bindingLayoutEntries{};

			BindGroupLayoutEntry uniformsBindingLayout = Default;
			// The binding index as used in the @binding attribute in the shader
			uniformsBindingLayout.binding = 0;
			// The stage that needs to access this resource
			uniformsBindingLayout.visibility = ShaderStage::Vertex;
			uniformsBindingLayout.buffer.type = BufferBindingType::Uniform;
			uniformsBindingLayout.buffer.minBindingSize = sizeof(Issam::NodeProperties);
			bindingLayoutEntries.push_back(uniformsBindingLayout);

			// Create a bind group layout
			BindGroupLayoutDescriptor bindGroupLayoutDesc{};
			bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();;
			bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
			m_bindGroupLayouts.push_back(Context::getInstance().getDevice().createBindGroupLayout(bindGroupLayoutDesc));
		}

		//Scene Uniforms
		{
			std::vector<BindGroupLayoutEntry> bindingLayoutEntries{};

			BindGroupLayoutEntry uniformsBindingLayout = Default;
			// The binding index as used in the @binding attribute in the shader
			uniformsBindingLayout.binding = 0;
			// The stage that needs to access this resource
			uniformsBindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
			uniformsBindingLayout.buffer.type = BufferBindingType::Uniform;
			uniformsBindingLayout.buffer.minBindingSize = sizeof(UniformsData);
			bindingLayoutEntries.push_back(uniformsBindingLayout);

			// Create a bind group layout
			BindGroupLayoutDescriptor bindGroupLayoutDesc{};
			bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();;
			bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
			m_bindGroupLayouts.push_back(Context::getInstance().getDevice().createBindGroupLayout(bindGroupLayoutDesc));
		}
		m_dirtyBindGroupLayouts = true;
		return m_bindGroupLayouts;
	}
	void setMaterialModule(MaterialModule* material) { m_materialModule = material; }
	void setScene(Issam::Scene* scene) { m_scene = scene; }
	MaterialModule* m_materialModule{ nullptr };
private:

	std::vector<BindGroupLayout> m_bindGroupLayouts{};
	bool m_dirtyBindGroupLayouts = true;

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
	std::vector<VertexAttr> m_vertexInputs{};
	std::vector<VertexAttr> m_vertexOutputs{};
	ShaderModule m_shaderModule{ nullptr };
	Issam::Scene* m_scene{ nullptr };
	bool m_dirtyShaderModule = true;
	
	std::string m_shaderSource;
};

//class ShaderManager
//{
//public:
//	ShaderManager() = default;
//	~ShaderManager() = default;
//
//	static ShaderManager& getInstance() {
//		static ShaderManager shaderManager;
//		return shaderManager;
//	};
//
//	bool add(const std::string& id, Shader* shader) {
//		m_shaders[id] = shader;
//		shader->build();
//		return true;
//	}
//	Shader* getShader(const std::string& id) { return  m_shaders[id]; }
//private:
//	std::unordered_map<std::string, Shader*> m_shaders{};
//};