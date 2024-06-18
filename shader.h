#pragma once



#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"
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
	}

	void build()
	{	
		{
			int binding = 0;
			std::vector<BindGroupLayoutEntry> bindingLayoutEntries{};

			BindGroupLayoutEntry uniformsBindingLayout = Default;
			// The binding index as used in the @binding attribute in the shader
			uniformsBindingLayout.binding = binding++;
			// The stage that needs to access this resource
			uniformsBindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
			uniformsBindingLayout.buffer.type = BufferBindingType::Uniform;
			uniformsBindingLayout.buffer.minBindingSize = sizeof(std::array<vec4, UNIFORMS_MAX>);
			bindingLayoutEntries.push_back(uniformsBindingLayout);

			for (const auto& texture : m_material->getTextures())
			{
				// The texture binding
				BindGroupLayoutEntry textureBindingLayout = Default;
				textureBindingLayout.binding = binding++;
				textureBindingLayout.visibility = ShaderStage::Fragment;
				textureBindingLayout.texture.sampleType = TextureSampleType::Float;
				textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;
				bindingLayoutEntries.push_back(textureBindingLayout);
			}

			for (const auto& sampler : m_material->getSamplers())
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

		//Camera Uniforms
		{
			std::vector<BindGroupLayoutEntry> bindingLayoutEntries{};

			BindGroupLayoutEntry uniformsBindingLayout = Default;
			// The binding index as used in the @binding attribute in the shader
			uniformsBindingLayout.binding = 0;
			// The stage that needs to access this resource
			uniformsBindingLayout.visibility = ShaderStage::Vertex;
			uniformsBindingLayout.buffer.type = BufferBindingType::Uniform;
			uniformsBindingLayout.buffer.minBindingSize = sizeof(Issam::CameraProperties);
			bindingLayoutEntries.push_back(uniformsBindingLayout);

			// Create a bind group layout
			BindGroupLayoutDescriptor bindGroupLayoutDesc{};
			bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();;
			bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
			m_bindGroupLayouts.push_back(Context::getInstance().getDevice().createBindGroupLayout(bindGroupLayoutDesc));
		}

	
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
		for (const auto& uniform : m_material->getUniforms())
		{
			vertexUniformsStr += "    " + uniform.name + ": " + (uniform.isMatrix() ? "mat4x4f" : "vec4f") + ", \n";
		}
		vertexUniformsStr += "}; \n \n";
		//Binding groups
		{
			vertexUniformsStr += "@group(0) @binding(0) var<uniform> u_uniforms: Uniforms;\n";
			int binding = 1; //0 used for uniforms
			for (const auto& texture : m_material->getTextures())
			{
				vertexUniformsStr += "@group(0) @binding(" + std::to_string(binding++) + ") var " + texture.first + " : texture_2d<f32>;\n";
			}
			for (const auto& sampler : m_material->getSamplers())
			{
				vertexUniformsStr += "@group(0) @binding(" + std::to_string(binding++) + ") var " + sampler.first + " : sampler;\n";
			}
			vertexUniformsStr += "\n \n";
		}

		vertexUniformsStr += "@group(1) @binding(0) var<uniform> u_model: mat4x4f;\n";


		std::string cameraStructStr = "struct Camera { \n";
		cameraStructStr += "    view: mat4x4f, \n";
		cameraStructStr += "    projection: mat4x4f, \n";
		cameraStructStr += "}; \n \n";
		vertexUniformsStr += cameraStructStr;
		vertexUniformsStr += "@group(2) @binding(0) var<uniform> u_camera: Camera;\n";

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
	}

	VertexState getVertexState()
	{
		VertexState vertexState;
		vertexState.module = m_shaderModule;
		vertexState.entryPoint = "vs_main";
		vertexState.constantCount = 0;
		vertexState.constants = nullptr;
		return vertexState;
	}

	FragmentState getFragmentState()
	{
		FragmentState fragmentState;
		fragmentState.module = m_shaderModule;
		fragmentState.entryPoint = "fs_main";
		fragmentState.constantCount = 0;
		fragmentState.constants = nullptr;
		return fragmentState;
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
	}

	void addVertexOutput(const std::string& inputName, int location, VertexFormat format)
	{
		m_vertexOutputs.push_back({ inputName , location,format });
	}

	


	const std::vector<BindGroupLayout>& getBindGroupLayout() const { return m_bindGroupLayouts; }
	void setMaterial(Issam::Material* material) { m_material = material; }
	Issam::Material* m_material{ nullptr };
private:

	std::vector<BindGroupLayout> m_bindGroupLayouts{};
	

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
	
	
	std::string m_shaderSource;
};