#pragma once
#include <variant>

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

#define UNIFORMS_MAX 15
using Value = std::variant<float, glm::vec4, glm::mat4>;
struct Uniform {
	std::string name;
	int handle;
	Value value;
	bool isMatrix() const { return std::holds_alternative< glm::mat4>(value); }
	/*bool isFloat() const { return std::holds_alternative< float>(value); }
	bool isVec4() const { return std::holds_alternative< glm::vec4>(value); }

	float getFloat() {
		assert(isFloat());
		return std::get<float>(value);
	}

	glm::vec4 getVec4() {
		assert(isVec4());
		return std::get<glm::vec4>(value);
	}

	glm::mat4x4 getMat4x4() {
		assert(isMatrix());
		return std::get<glm::mat4x4>(value);
	}*/
};


class UniformsBuffer
{
public:
	UniformsBuffer()
	{
		BufferDescriptor bufferDesc;
		bufferDesc.size = sizeof(std::array<glm::vec4, UNIFORMS_MAX>);
		// Make sure to flag the buffer as BufferUsage::Uniform
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
		bufferDesc.mappedAtCreation = false;
		m_uniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

		Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(std::array<vec4, UNIFORMS_MAX>));
	};
	~UniformsBuffer() = default;

	uint16_t allocateVec4() {
		assert(m_uniformIndex < UNIFORMS_MAX);
		return m_uniformIndex++;
	}

	uint16_t allocateMat4()
	{
		assert(m_uniformIndex + 4 < UNIFORMS_MAX);
		uint16_t currentHandle = m_uniformIndex;
		m_uniformIndex += 4;
		return currentHandle;
	}

	void set(uint16_t handle, const Value& value)
	{
		if (std::holds_alternative< glm::vec4>(value))
			m_uniformsData[handle] = std::get<glm::vec4>(value);
		else if (std::holds_alternative< float>(value))
		{
			float newValue = std::get<float>(value);
			m_uniformsData[handle] = glm::vec4(newValue, newValue, newValue, newValue);
		}
		else if (std::holds_alternative< glm::mat4x4>(value))
		{

			mat4x4 mat = std::get<mat4x4>(value);
			m_uniformsData[handle] = { mat[0][0], mat[0][1], mat[0][2], mat[0][3] };
			m_uniformsData[handle + 1] = { mat[1][0], mat[1][1], mat[1][2], mat[1][3] };
			m_uniformsData[handle + 2] = { mat[2][0], mat[2][1], mat[2][2], mat[2][3] };
			m_uniformsData[handle + 3] = { mat[3][0], mat[3][1], mat[3][2], mat[3][3] };
		}
		else
			assert(false);
		Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(std::array<vec4, UNIFORMS_MAX>)); //TODO envoyer que la partie modifie
	}

	Buffer getBuffer() { return m_uniformBuffer; }

private:
	std::array<vec4, UNIFORMS_MAX> m_uniformsData{};
	uint16_t m_uniformIndex = 0;
	Buffer m_uniformBuffer{ nullptr };
};



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

			for (const auto& texture : m_textures)
			{
				// The texture binding
				BindGroupLayoutEntry textureBindingLayout = Default;
				textureBindingLayout.binding = binding++;
				textureBindingLayout.visibility = ShaderStage::Fragment;
				textureBindingLayout.texture.sampleType = TextureSampleType::Float;
				textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;
				bindingLayoutEntries.push_back(textureBindingLayout);
			}

			for (const auto& sampler : m_samplers)
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
		for (const auto& uniform : m_uniforms)
		{
			vertexUniformsStr += "    " + uniform.name + ": " + (uniform.isMatrix() ? "mat4x4f" : "vec4f") + ", \n";
		}
		vertexUniformsStr += "}; \n \n";
		//Binding groups
		{
			vertexUniformsStr += "@group(0) @binding(0) var<uniform> u_uniforms: Uniforms;\n";
			int binding = 1; //0 used for uniforms
			for (const auto& texture : m_textures)
			{
				vertexUniformsStr += "@group(0) @binding(" + std::to_string(binding++) + ") var " + texture.first + " : texture_2d<f32>;\n";
			}
			for (const auto& sampler : m_samplers)
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
		m_uniformsBuffer.set(uniform.handle, value);
		m_dirtyBindGroup = true;
	}


	const std::vector<BindGroupLayout>& getBindGroupLayout() const { return m_bindGroupLayouts; }
	BindGroup getBindGroup() {
		if (m_dirtyBindGroup)
			refreshBinding();
		return m_bindGroup;
	}

private:

	void refreshBinding()
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
		bindGroupDesc.layout = m_bindGroupLayouts.at(0);
		// There must be as many bindings as declared in the layout!
		bindGroupDesc.entryCount = (uint32_t)bindings.size();
		bindGroupDesc.entries = bindings.data();
		m_bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
		m_dirtyBindGroup = false;
	}

	bool m_dirtyBindGroup = true;
	std::vector<BindGroupLayout> m_bindGroupLayouts{};
	BindGroup m_bindGroup{ nullptr };
	std::vector<std::pair<std::string, TextureView> > m_textures{};
	std::vector<std::pair<std::string, Sampler>> m_samplers{};

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
	std::vector<Uniform> m_uniforms{};
	UniformsBuffer m_uniformsBuffer{};
	std::string m_shaderSource;
};