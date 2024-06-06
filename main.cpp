/**
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://eliemichel.github.io/LearnWebGPU
 *
 * MIT License
 * Copyright (c) 2022-2023 Elie Michel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#define TINYOBJLOADER_IMPLEMENTATION // add this to exactly 1 of your C++ files
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <array>
#include <variant>
#include "webgpu-utils.h"

using namespace wgpu;
namespace fs = std::filesystem;

using namespace glm;

constexpr float PI = 3.14159265358979323846f;

#ifdef WEBGPU_BACKEND_WGPU
TextureFormat swapChainFormat = surface.getPreferredFormat(adapter);
#else
TextureFormat swapChainFormat = TextureFormat::BGRA8Unorm;
#endif

TextureFormat depthTextureFormat = TextureFormat::Depth24Plus;

class Context
{
public:
	static Context& Context::getInstance()
	{
		static Context ctx;
		return ctx;
	}
	
	~Context() = default;


	void initGraphics(GLFWwindow* window, uint16_t width, uint16_t height)
	{
		m_instance = createInstance(InstanceDescriptor{});
		if (!m_instance) {
			std::cerr << "Could not initialize WebGPU!" << std::endl;
			return;
		}

		//Requesting adapter...
		m_surface = glfwGetWGPUSurface(m_instance, window);
		RequestAdapterOptions adapterOpts;
		adapterOpts.compatibleSurface = m_surface;
		m_adapter = m_instance.requestAdapter(adapterOpts);
		assert(m_adapter);
	//	inspectAdapter(m_adapter);

		//Requesting device...
		DeviceDescriptor deviceDesc;
		deviceDesc.label = "My Device";
		deviceDesc.requiredFeaturesCount = 0;
		deviceDesc.requiredLimits = nullptr;
		deviceDesc.defaultQueue.label = "The default queue";
		m_device = m_adapter.requestDevice(deviceDesc);
		assert(m_device);
	//	inspectDevice(m_device);

		// Add an error callback for more debug info
		m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
			std::cout << "Device error: type " << type;
			if (message) std::cout << " (message: " << message << ")";
			std::cout << std::endl;
		});
		
		auto h2 = m_device.setLoggingCallback([](LoggingType type, char const* message) {
			std::cout << "Device error: type " << type;
			if (message) std::cout << " (message: " << message << ")";
			std::cout << std::endl;
		});

		

		//Creating swapchain...
		SwapChainDescriptor swapChainDesc;
		swapChainDesc.width = width;
		swapChainDesc.height = height;
		swapChainDesc.usage = TextureUsage::RenderAttachment;
		swapChainDesc.format = swapChainFormat;
		swapChainDesc.presentMode = PresentMode::Fifo;
		m_swapChain = m_device.createSwapChain(m_surface, swapChainDesc);
		assert(m_swapChain);
	}

	std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;

	void shutdownGraphics()
	{
		m_swapChain.release();
		m_device.release();
		m_adapter.release();
		m_instance.release();
		m_surface.release();
	}

	Device getDevice() { return m_device; }
	SwapChain getSwapChain() { return m_swapChain; }

private:
	Context() = default;

	Device m_device = nullptr;
	SwapChain m_swapChain = nullptr;
	Instance m_instance = nullptr;
	Surface m_surface = nullptr;
	Adapter m_adapter = nullptr;
};

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

		for(const auto& texture : m_textures)
		{
			// The texture binding
			BindGroupLayoutEntry textureBindingLayout = Default;
			textureBindingLayout.binding = binding++;
			textureBindingLayout.visibility = ShaderStage::Fragment;
			textureBindingLayout.texture.sampleType = TextureSampleType::Float;
			textureBindingLayout.texture.viewDimension = TextureViewDimension::_2D;
			bindingLayoutEntries.push_back(textureBindingLayout);
		}

		for (const auto& sampler: m_samplers)
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
		m_bindGroupLayout = Context::getInstance().getDevice().createBindGroupLayout(bindGroupLayoutDesc);

		binding = 0;
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
		bindGroupDesc.layout = m_bindGroupLayout;
		// There must be as many bindings as declared in the layout!
		bindGroupDesc.entryCount = (uint32_t)bindings.size();
		bindGroupDesc.entries = bindings.data();
		m_bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);


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

	void addTexture(const std::string& name, TextureView texturView) { m_textures.push_back({ name, texturView }); }
	void addSampler(const std::string& name, Sampler sampler) { m_samplers.push_back({ name, sampler }); }

	void addUniform(std::string name) {
		uint16_t handle =m_uniformsBuffer.allocateVec4();
		m_uniforms.push_back({ name, handle, glm::vec4(0.0f)});
	};
		
	void addUniformMatrix(std::string name) {
		uint16_t handle = m_uniformsBuffer.allocateMat4();
		m_uniforms.push_back({ name, handle, glm::mat4x4(1.0f) });
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
	}
	

	BindGroupLayout getBindGroupLayout() { return m_bindGroupLayout; }
	BindGroup getBindGroup() {return m_bindGroup; }

private:
	BindGroupLayout m_bindGroupLayout{ nullptr };
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

//https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/hello-triangle.html#render-pipeline
class Pipeline
{
public:
	Pipeline(Shader* shader, std::vector<VertexBufferLayout> vertexBufferLayouts) {
		//Creating render pipeline
		RenderPipelineDescriptor pipelineDesc;

		// Vertex fetch
		pipelineDesc.vertex = shader->getVertexState();
		pipelineDesc.vertex.bufferCount = static_cast<uint32_t>(vertexBufferLayouts.size());
		pipelineDesc.vertex.buffers = vertexBufferLayouts.data();

		// Primitive assembly and rasterization
		pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
		pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
		pipelineDesc.primitive.frontFace = FrontFace::CCW;
		pipelineDesc.primitive.cullMode = CullMode::None;

		// Fragment shader
		FragmentState fragmentState = shader->getFragmentState();
		pipelineDesc.fragment = &fragmentState;


		// Configure blend state
		BlendState blendState;
		// Usual alpha blending for the color:
		blendState.color.srcFactor = BlendFactor::SrcAlpha;
		blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
		blendState.color.operation = BlendOperation::Add;
		// We leave the target alpha untouched:
		blendState.alpha.srcFactor = BlendFactor::Zero;
		blendState.alpha.dstFactor = BlendFactor::One;
		blendState.alpha.operation = BlendOperation::Add;

		ColorTargetState colorTarget;
		colorTarget.format = swapChainFormat;
		colorTarget.blend = &blendState;
		colorTarget.writeMask = ColorWriteMask::All; // We could write to only some of the color channels.

		// We have only one target because our render pass has only one output color
		// attachment.
		fragmentState.targets = &colorTarget;
		fragmentState.targetCount = 1;

		// Depth and stencil tests are not used here
		//pipelineDesc.depthStencil = nullptr;
		DepthStencilState depthStencilState = Default;
		// Setup depth state
		depthStencilState.depthCompare = CompareFunction::Less;
		// Each time a fragment is blended into the target, we update the value of the Z-buffer
		depthStencilState.depthWriteEnabled = true;
		// Store the format in a variable as later parts of the code depend on it
		depthStencilState.format = depthTextureFormat;
		// Deactivate the stencil alltogether
		depthStencilState.stencilReadMask = 0;
		depthStencilState.stencilWriteMask = 0;

		pipelineDesc.depthStencil = &depthStencilState;

		// Multi-sampling
		// Samples per pixel
		pipelineDesc.multisample.count = 1;
		// Default value for the mask, meaning "all bits on"
		pipelineDesc.multisample.mask = ~0u;
		// Default value as well (irrelevant for count = 1 anyways)
		pipelineDesc.multisample.alphaToCoverageEnabled = false;

		// Pipeline layout
		// Create the pipeline layout
		PipelineLayoutDescriptor layoutDesc{};
		layoutDesc.bindGroupLayoutCount = 1;
		layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&shader->getBindGroupLayout();
		PipelineLayout layout = Context::getInstance().getDevice().createPipelineLayout(layoutDesc);
		pipelineDesc.layout = layout;

		m_pipeline = Context::getInstance().getDevice().createRenderPipeline(pipelineDesc);
	};
	~Pipeline() {
		m_pipeline.release();
	};
	
	RenderPipeline getRenderPipeline() const { return m_pipeline; }
private:
	RenderPipeline m_pipeline{ nullptr };
};

class Pass
{
public:
	Pass(Shader* shader):
		m_shader(shader)
	{
	};
	~Pass() {
		m_depthTextureView.release();
		//depthTexture.destroy(); TODO
		//depthTexture.release();
		delete depthStencilAttachment;
		delete renderPassColorAttachment;
	};

	Shader* getShader() const { return m_shader; }
	
	void addDepthBuffer(uint32_t width, uint32_t height, WGPUTextureFormat format)
	{
		// Create the depth texture
		TextureDescriptor depthTextureDesc;
		depthTextureDesc.dimension = TextureDimension::_2D;
		depthTextureDesc.format = format;
		depthTextureDesc.mipLevelCount = 1;
		depthTextureDesc.sampleCount = 1;
		depthTextureDesc.size = { width, height, 1 };
		depthTextureDesc.usage = TextureUsage::RenderAttachment;
		depthTextureDesc.viewFormatCount = 1;
		depthTextureDesc.viewFormats = (WGPUTextureFormat*)&format;
		Texture depthTexture = Context::getInstance().getDevice().createTexture(depthTextureDesc);

		// Create the view of the depth texture manipulated by the rasterizer
		TextureViewDescriptor depthTextureViewDesc;
		depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
		depthTextureViewDesc.baseArrayLayer = 0;
		depthTextureViewDesc.arrayLayerCount = 1;
		depthTextureViewDesc.baseMipLevel = 0;
		depthTextureViewDesc.mipLevelCount = 1;
		depthTextureViewDesc.dimension = TextureViewDimension::_2D;
		depthTextureViewDesc.format = format;
		m_depthTextureView = depthTexture.createView(depthTextureViewDesc);
	}

	//const TextureView getDepthTextureView() const { return m_depthTextureView; }
	const RenderPassDepthStencilAttachment* getRenderPassDepthStencilAttachment(){
		if (m_depthTextureView)
		{
			depthStencilAttachment = new RenderPassDepthStencilAttachment(); 
			// The view of the depth texture
			depthStencilAttachment->view = m_depthTextureView;

			// The initial value of the depth buffer, meaning "far"
			depthStencilAttachment->depthClearValue = 1.0f;
			// Operation settings comparable to the color attachment
			depthStencilAttachment->depthLoadOp = LoadOp::Clear;
			depthStencilAttachment->depthStoreOp = StoreOp::Store;
			// we could turn off writing to the depth buffer globally here
			depthStencilAttachment->depthReadOnly = false;

			// Stencil setup, mandatory but unused
			depthStencilAttachment->stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
			depthStencilAttachment->stencilLoadOp = LoadOp::Clear;
			depthStencilAttachment->stencilStoreOp = StoreOp::Store;
#else
			depthStencilAttachment->stencilLoadOp = LoadOp::Undefined;
			depthStencilAttachment->stencilStoreOp = StoreOp::Undefined;
#endif
			depthStencilAttachment->stencilReadOnly = true;
			return depthStencilAttachment;
		}
		else
		{
			return nullptr;
		}
	}

	const RenderPassColorAttachment* getRenderPassColorAttachment(WGPUTextureView view) {
		renderPassColorAttachment = new RenderPassColorAttachment();
		renderPassColorAttachment->view = view;
		renderPassColorAttachment->resolveTarget = nullptr;
		renderPassColorAttachment->loadOp = LoadOp::Clear;
		renderPassColorAttachment->storeOp = StoreOp::Store;
		renderPassColorAttachment->clearValue = Color{ 0.3, 0.3, 0.3, 1.0 };
		return renderPassColorAttachment;
	}

private:
	RenderPassDepthStencilAttachment* depthStencilAttachment;
	RenderPassColorAttachment* renderPassColorAttachment;
	Shader* m_shader { nullptr };
	TextureView m_depthTextureView{ nullptr };
};

class VertexBuffer
{
public:
	VertexBuffer(void* vertexData, size_t size, int vertexCount):
		m_data (vertexData),
		m_count(vertexCount)
	{
		// Create vertex buffer
		m_size = size;
		BufferDescriptor bufferDesc;
		bufferDesc.size = m_size;
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
		bufferDesc.mappedAtCreation = false;
		m_buffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

		// Upload geometry data to the buffer
		Context::getInstance().getDevice().getQueue().writeBuffer(m_buffer, 0, m_data, bufferDesc.size);
	};
	
	~VertexBuffer() {};

	void addVertexAttrib(const std::string& attribName, int location, VertexFormat format, int offset)
	{
		VertexAttribute vertexAttribute;
		vertexAttribute.shaderLocation = location;
		vertexAttribute.format = format;
		vertexAttribute.offset = offset;
		m_vertexAttribs.push_back(vertexAttribute);
	}
	void setAttribsStride(uint64_t attribsStride) { m_attribsStride = attribsStride; }


	const Buffer& getBuffer()const { return m_buffer; }
	const uint64_t getSize() const { return m_size; }
	const int getCount() const { return m_count; }

private:
	Buffer m_buffer{ nullptr };
	void* m_data;
	size_t  m_size;
	int m_count{ 0 };
	std::vector<VertexAttribute> m_vertexAttribs;
	uint64_t m_attribsStride = 0;

	friend class Mesh;
};

class IndexBuffer
{
public:
	IndexBuffer(void* indexData, size_t size, int indexCount) :
		m_data(indexData),
		m_count(indexCount)
	{
		m_size = size * sizeof(uint16_t);
		BufferDescriptor bufferDesc;
		bufferDesc.size = m_size;
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;

		bufferDesc.size = (bufferDesc.size + 3) & ~3; // round up to the next multiple of 4
		m_buffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

		// Upload geometry data to the buffer
		Context::getInstance().getDevice().getQueue().writeBuffer(m_buffer, 0, m_data, bufferDesc.size);
	};
	~IndexBuffer() {};

	const Buffer& getBuffer()const { return m_buffer; }
	const size_t getSize() const { return m_size; }
	const int getCount() const { return m_count; }

private:
	Buffer m_buffer{ nullptr };
	void* m_data;
	size_t  m_size;
	int m_count{ 0 };
};


class Mesh
{
public:
	Mesh(){};
	~Mesh() {};
	void addVertexBuffer(VertexBuffer* vertexBuffer) { m_vertexBuffers.push_back(vertexBuffer); }
	void addIndexBuffer(IndexBuffer* indexBuffer) { m_indexBuffer = indexBuffer; }

	std::vector<VertexBuffer*> getVertexBuffers() { return  m_vertexBuffers; }
	IndexBuffer* getIndexBuffer() { return m_indexBuffer; };

	int getVertexCount() { return m_vertexBuffers[0]->getCount(); } //il faut que tout les vertexBuffer aillent le meme count !!

	const std::vector<VertexBufferLayout> getVertexBufferLayouts() {
		std::vector<VertexBufferLayout> vertexBufferLayouts;
		for (auto vb : m_vertexBuffers)
		{
			VertexBufferLayout vertexBufferLayout;
			// [...] Build vertex buffer layout
			vertexBufferLayout.attributeCount = (uint32_t)vb->m_vertexAttribs.size();
			vertexBufferLayout.attributes = vb->m_vertexAttribs.data();
			// == Common to attributes from the same buffer ==
			vertexBufferLayout.arrayStride = vb->m_attribsStride;
			vertexBufferLayout.stepMode = VertexStepMode::Vertex;
			vertexBufferLayouts.push_back(vertexBufferLayout);
		}
		return vertexBufferLayouts;
	}
private:

	std::vector<VertexBuffer*> m_vertexBuffers;
	IndexBuffer* m_indexBuffer{nullptr};
};

class MeshManager
{
public:
	///
	static MeshManager& getInstance() {
		static MeshManager meshManager;
		return meshManager;
	};

	bool add(const std::string& id, Mesh* mesh) { m_meshes[id] = mesh; return true; }
	Mesh* get(const std::string& id) { return  m_meshes[id]; }
	std::unordered_map<std::string, Mesh*> getAll() { return m_meshes; }
private:
	std::unordered_map<std::string, Mesh*> m_meshes;
};


class Renderer
{
public:
	Renderer()
	{
		m_queue = Context::getInstance().getDevice().getQueue();	
	};
	~Renderer() = default;

	void draw()
	{
		CommandEncoderDescriptor commandEncoderDesc;
		commandEncoderDesc.label = "Command Encoder";
		CommandEncoder encoder = Context::getInstance().getDevice().createCommandEncoder(commandEncoderDesc);
		
		TextureView nextTexture = Context::getInstance().getSwapChain().getCurrentTextureView();
		if (!nextTexture) {
			std::cerr << "Cannot acquire next swap chain texture" << std::endl;
		}

		for (auto& pass : m_passes)
		{
			RenderPassDescriptor renderPassDesc;

			renderPassDesc.colorAttachmentCount = 1;
			renderPassDesc.colorAttachments = pass.getRenderPassColorAttachment(nextTexture);//&renderPassColorAttachment;
			renderPassDesc.depthStencilAttachment = pass.getRenderPassDepthStencilAttachment();

			renderPassDesc.timestampWriteCount = 0;
			renderPassDesc.timestampWrites = nullptr;
			
			RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
			renderPass.pushDebugGroup("Render Pass");
			for (auto [meshId , mesh] : MeshManager::getInstance().getAll())
			{
				Pipeline* pipeline = new Pipeline(pass.getShader(), mesh->getVertexBufferLayouts()); //TODO le creer le moins possible
				renderPass.setPipeline(pipeline->getRenderPipeline());
				renderPass.setBindGroup(0, pass.getShader()->getBindGroup(), 0, nullptr);
				int slot = 0; //The first argument(slot) corresponds to the index of the buffer layout in the pipelineDesc.vertex.buffers array.
				for (const auto& vb : mesh->getVertexBuffers())
				{
					renderPass.setVertexBuffer(slot, vb->getBuffer(), 0, vb->getSize());
					slot++;
				}
				if (mesh->getIndexBuffer() != nullptr)
				{
					renderPass.setIndexBuffer(mesh->getIndexBuffer()->getBuffer(), IndexFormat::Uint16, 0, mesh->getIndexBuffer()->getSize());
					renderPass.drawIndexed(mesh->getIndexBuffer()->getCount(), 1, 0, 0, 0);
				}
				else
					renderPass.draw(mesh->getVertexCount(), 1, 0, 0);

				delete pipeline;

			}
			renderPass.popDebugGroup();

			renderPass.end();
			renderPass.release();
		}
		Context::getInstance().getDevice().tick();

		CommandBufferDescriptor cmdBufferDescriptor;
		cmdBufferDescriptor.label = "Command buffer";
		CommandBuffer command = encoder.finish(cmdBufferDescriptor);
		encoder.release();
		m_queue.submit(command);
		command.release();

		Context::getInstance().getSwapChain().getCurrentTextureView().release();
		Context::getInstance().getSwapChain().present();
	};

	void addPass(const Pass& pass)
	{
		m_passes.push_back(pass);
	}


private:
	Queue m_queue{ nullptr };
	std::vector<Pass> m_passes;
};


int m_winWidth = 640;
int m_winHeight = 480;

void addTwoTriangles()
{
	std::vector<float> positionData = {
	-0.5, -0.5,
	+0.5, -0.5,
	+0.0, +0.5,
	-0.55f, -0.5,
	-0.05f, +0.5,
	-0.55f, +0.5
	};

	// r0,  g0,  b0, r1,  g1,  b1, ...
	std::vector<float> colorData = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0,
		1.0, 1.0, 0.0,
		1.0, 0.0, 1.0,
		0.0, 1.0, 1.0
	};

	int vertexCount = static_cast<int>(positionData.size() / 2);
	VertexBuffer* vertexBufferPos = new VertexBuffer(positionData.data(), positionData.size() * sizeof(float), vertexCount);
	vertexBufferPos->addVertexAttrib("Position", 0, VertexFormat::Float32x2, 0);
	vertexBufferPos->setAttribsStride(2 * sizeof(float));

	VertexBuffer* vertexBufferColor = new VertexBuffer(colorData.data(), colorData.size() * sizeof(float), vertexCount);
	vertexBufferColor->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 0); // offset = sizeof(Position)
	vertexBufferColor->setAttribsStride(3 * sizeof(float));

	Mesh* mesh = new Mesh();
	mesh->addVertexBuffer(vertexBufferPos);
	mesh->addVertexBuffer(vertexBufferColor);

	//interleaved  mesh
	//std::vector<float> vertexData = {
	//	// x0,  y0,  r0,  g0,  b0
	//	-0.5, -0.5, 1.0, 0.0, 0.0,

	//	// x1,  y1,  r1,  g1,  b1
	//	+0.5, -0.5, 0.0, 1.0, 0.0,

	//	// ...
	//	+0.0,   +0.5, 0.0, 0.0, 1.0,
	//	-0.55f, -0.5, 1.0, 1.0, 0.0,
	//	-0.05f, +0.5, 1.0, 0.0, 1.0,
	//	-0.55f, +0.5, 0.0, 1.0, 1.0
	//};
	//int vertexCount = static_cast<int>(vertexData.size() / 5);

	//VertexBuffer* vertexBuffer = new VertexBuffer(vertexData, vertexCount); // vertexData.size()* sizeof(float)
	//vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x2, 0);
	//vertexBuffer->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 2 * sizeof(float)); // offset = sizeof(Position)
	//vertexBuffer->setAttribsStride(5 * sizeof(float));
	//Mesh* mesh = new Mesh();
	//mesh->addVertexBuffer(vertexBuffer);


	MeshManager::getInstance().add("twoTriangles", mesh);
}


void addColoredPlane() {
	std::vector<float> pointData = {
		// x,   y,     r,   g,   b
		-0.5, -0.5,   1.0, 0.0, 0.0,
		+0.5, -0.5,   0.0, 1.0, 0.0,
		+0.5, +0.5,   0.0, 0.0, 1.0,
		-0.5, +0.5,   1.0, 1.0, 0.0
	};

	std::vector<uint16_t> indexData = {
	0, 1, 2, // Triangle #0
	0, 2, 3  // Triangle #1
	};

	int indexCount = static_cast<int>(indexData.size());
	int vertexCount = static_cast<int>(pointData.size() / 5);

	VertexBuffer* vertexBuffer = new VertexBuffer(pointData.data(), pointData.size() * sizeof(float), vertexCount);
	vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x2, 0);
	vertexBuffer->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 2 * sizeof(float)); // offset = sizeof(Position)
	vertexBuffer->setAttribsStride(5 * sizeof(float));

	IndexBuffer* indexBuffer = new IndexBuffer(indexData.data(), indexData.size(), indexCount);
	Mesh* mesh = new Mesh();
	mesh->addVertexBuffer(vertexBuffer);
	mesh->addIndexBuffer(indexBuffer);
	MeshManager::getInstance().add("ColoredPlane", mesh);
}

void addPyramid()
{
	std::vector<float> pointData = {
		-0.5f, -0.5f, -0.3f,    1.0f, 1.0f, 1.0f,
		+0.5f, -0.5f, -0.3f,    1.0f, 1.0f, 1.0f,
		+0.5f, +0.5f, -0.3f,    1.0f, 1.0f, 1.0f,
		-0.5f, +0.5f, -0.3f,    1.0f, 1.0f, 1.0f,
		+0.0f, +0.0f, +0.5f,    0.5f, 0.5f, 0.5f,
	};
	std::vector<uint16_t> indexData = {
		0,  1,  2,
		0,  2,  3,
		0,  1,  4,
		1,  2,  4,
		2,  3,  4,
		3,  0,  4,
	};

	int indexCount = static_cast<int>(indexData.size());
	int vertexCount = static_cast<int>(pointData.size() / 6);

	VertexBuffer* vertexBuffer = new VertexBuffer(pointData.data(), pointData.size() * sizeof(float), vertexCount);
	vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x3, 0);
	vertexBuffer->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 3 * sizeof(float)); // offset = sizeof(Position)
	vertexBuffer->setAttribsStride(6 * sizeof(float));

	IndexBuffer* indexBuffer = new IndexBuffer(indexData.data(), indexData.size(), indexCount);
	Mesh* mesh = new Mesh();
	mesh->addVertexBuffer(vertexBuffer);
	mesh->addIndexBuffer(indexBuffer);
	MeshManager::getInstance().add("Pyramid", mesh);
}

struct VertexAttributes {
	vec3 position;
	vec3 normal;
	vec3 color;
	vec2 uv;
};

bool loadGeometryFromObj(const fs::path& path) {
	std::vector<VertexAttributes> vertexData;
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	// Call the core loading procedure of TinyOBJLoader
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str());

	// Check errors
	if (!warn.empty()) {
		std::cout << warn << std::endl;
	}

	if (!err.empty()) {
		std::cerr << err << std::endl;
	}

	if (!ret) {
		return false;
	}

	// Filling in vertexData:
	vertexData.clear();
	for (const auto& shape : shapes) {
		size_t offset = vertexData.size();
		vertexData.resize(offset + shape.mesh.indices.size());

		for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
			const tinyobj::index_t& idx = shape.mesh.indices[i];

			vertexData[offset + i].position = {
				attrib.vertices[3 * idx.vertex_index + 0],
				-attrib.vertices[3 * idx.vertex_index + 2], // Add a minus to avoid mirroring
				attrib.vertices[3 * idx.vertex_index + 1]
			};

			// Also apply the transform to normals!!
			vertexData[offset + i].normal = {
				attrib.normals[3 * idx.normal_index + 0],
				-attrib.normals[3 * idx.normal_index + 2],
				attrib.normals[3 * idx.normal_index + 1]
			};

			vertexData[offset + i].color = {
				attrib.colors[3 * idx.vertex_index + 0],
				attrib.colors[3 * idx.vertex_index + 1],
				attrib.colors[3 * idx.vertex_index + 2]
			};

			vertexData[offset + i].uv = {
				attrib.texcoords[2 * idx.texcoord_index + 0],
				1 - attrib.texcoords[2 * idx.texcoord_index + 1]
			};
		}
	}

	int indexCount = static_cast<int>(vertexData.size());
	int vertexCount = static_cast<int>(vertexData.size() );
	auto* data = vertexData.data();
	VertexBuffer* vertexBuffer = new VertexBuffer(vertexData.data(), vertexData.size() *sizeof(VertexAttributes), vertexCount);
	vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x3, 0);
	vertexBuffer->addVertexAttrib("Normal", 1, VertexFormat::Float32x3, 3 * sizeof(float));
	vertexBuffer->addVertexAttrib("Color", 2, VertexFormat::Float32x3, 6 * sizeof(float)); 
	vertexBuffer->addVertexAttrib("TexCoord", 3, VertexFormat::Float32x2, 9 * sizeof(float));
	vertexBuffer->setAttribsStride(11 * sizeof(float));

	
	Mesh* mesh = new Mesh();
	mesh->addVertexBuffer(vertexBuffer);
	MeshManager::getInstance().add("obj_1", mesh);

	return true;
}

// Auxiliary function for loadTexture
static void writeMipMaps(
	Texture texture,
	Extent3D textureSize,
	uint32_t mipLevelCount,
	const unsigned char* pixelData)
{
	Queue queue = Context::getInstance().getDevice().getQueue();

	// Arguments telling which part of the texture we upload to
	ImageCopyTexture destination;
	destination.texture = texture;
	destination.origin = { 0, 0, 0 };
	destination.aspect = TextureAspect::All;

	// Arguments telling how the C++ side pixel memory is laid out
	TextureDataLayout source;
	source.offset = 0;

	// Create image data
	Extent3D mipLevelSize = textureSize;
	std::vector<unsigned char> previousLevelPixels;
	Extent3D previousMipLevelSize;
	for (uint32_t level = 0; level < mipLevelCount; ++level) {
		// Pixel data for the current level
		std::vector<unsigned char> pixels(4 * mipLevelSize.width * mipLevelSize.height);
		if (level == 0) {
			// We cannot really avoid this copy since we need this
			// in previousLevelPixels at the next iteration
			memcpy(pixels.data(), pixelData, pixels.size());
		}
		else {
			// Create mip level data
			for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
				for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
					unsigned char* p = &pixels[4 * (j * mipLevelSize.width + i)];
					// Get the corresponding 4 pixels from the previous level
					unsigned char* p00 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 0))];
					unsigned char* p01 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 1))];
					unsigned char* p10 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 0))];
					unsigned char* p11 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 1))];
					// Average
					p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
					p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
					p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
					p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / 4;
				}
			}
		}

		// Upload data to the GPU texture
		destination.mipLevel = level;
		source.bytesPerRow = 4 * mipLevelSize.width;
		source.rowsPerImage = mipLevelSize.height;
		queue.writeTexture(destination, pixels.data(), pixels.size(), source, mipLevelSize);

		previousLevelPixels = std::move(pixels);
		previousMipLevelSize = mipLevelSize;
		mipLevelSize.width /= 2;
		mipLevelSize.height /= 2;
	}

	queue.release();
}

static uint32_t bit_width(uint32_t m) {
	if (m == 0) return 0;
	else { uint32_t w = 0; while (m >>= 1) ++w; return w; }
}

Texture loadTexture(const fs::path& path, TextureView* pTextureView) {
	int width, height, channels;
	unsigned char* pixelData = stbi_load(path.string().c_str(), &width, &height, &channels, 4 /* force 4 channels */);
	// If data is null, loading failed.
	if (nullptr == pixelData) return nullptr;

	// Use the width, height, channels and data variables here
	TextureDescriptor textureDesc;
	textureDesc.dimension = TextureDimension::_2D;
	textureDesc.format = TextureFormat::RGBA8Unorm; // by convention for bmp, png and jpg file. Be careful with other formats.
	textureDesc.size = { (unsigned int)width, (unsigned int)height, 1 };
	textureDesc.mipLevelCount = bit_width(std::max(textureDesc.size.width, textureDesc.size.height));
	textureDesc.sampleCount = 1;
	textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
	textureDesc.viewFormatCount = 0;
	textureDesc.viewFormats = nullptr;
	Texture texture = Context::getInstance().getDevice().createTexture(textureDesc);

	// Upload data to the GPU texture
	writeMipMaps(texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

	stbi_image_free(pixelData);
	// (Do not use data after this)

	if (pTextureView) {
		TextureViewDescriptor textureViewDesc;
		textureViewDesc.aspect = TextureAspect::All;
		textureViewDesc.baseArrayLayer = 0;
		textureViewDesc.arrayLayerCount = 1;
		textureViewDesc.baseMipLevel = 0;
		textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
		textureViewDesc.dimension = TextureViewDimension::_2D;
		textureViewDesc.format = textureDesc.format;
		*pTextureView = texture.createView(textureViewDesc);
	}

	return texture;
}

std::string loadFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		std::cerr << path.c_str() << " not found ! " << std::endl;
	}
	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string str = std::string(size, ' ');
	file.seekg(0);
	file.read(str.data(), size);
	return str;
};

struct CameraState {
	// angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
	// angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
	vec2 angles = { 0.8f, 0.5f };
	// zoom is the position of the camera along its local forward axis, affected by the scroll wheel
	float zoom = -1.2f;
};

struct DragState {
	// Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
	bool active = false;
	// The position of the mouse at the beginning of the drag action
	vec2 startMouse;
	// The camera state at the beginning of the drag action
	CameraState startCameraState;

	// Constant settings
	float sensitivity = 0.01f;
	float scrollSensitivity = 0.1f;

	// Inertia
	vec2 velocity = { 0.0, 0.0 };
	vec2 previousDelta;
	float intertia = 0.9f;
};


DragState m_drag;
CameraState m_cameraState;

void updateViewMatrix(Shader* shader) {
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	mat4x4 viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1));
	shader->setUniform("view", viewMatrix);

}


int main(int, char**) {
	if (!glfwInit()) {
		std::cerr << "Could not initialize GLFW!" << std::endl;
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(m_winWidth, m_winHeight, "Learn WebGPU", NULL, NULL);
	if (!window) {
		std::cerr << "Could not open window!" << std::endl;
		return 1;
	}

	Context::getInstance().initGraphics(window, m_winWidth, m_winHeight);

	//addTwoTriangles();
	//addColoredPlane();
	//addPyramid();
	loadGeometryFromObj(DATA_DIR  "/fourareen.obj");

	TextureView textureView = nullptr;
	Texture texture = loadTexture(DATA_DIR "/fourareen2K_albedo.jpg", &textureView);

	// Create a sampler
	SamplerDescriptor samplerDesc;
	samplerDesc.addressModeU = AddressMode::Repeat;
	samplerDesc.addressModeV = AddressMode::Repeat;
	samplerDesc.addressModeW = AddressMode::Repeat;
	samplerDesc.magFilter = FilterMode::Linear;
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
	samplerDesc.lodMinClamp = 0.0f;
	samplerDesc.lodMaxClamp = 8.0f;
	samplerDesc.compare = CompareFunction::Undefined;
	samplerDesc.maxAnisotropy = 1;
	Sampler defaultSampler = Context::getInstance().getDevice().createSampler(samplerDesc);
    
	std::string userCode = loadFile(DATA_DIR  "/sahder_1.wgsl");
	Shader* shader_1 = new Shader();
	shader_1->setUserCode(userCode);
	shader_1->addVertexInput("position", 0, VertexFormat::Float32x3);
	shader_1->addVertexInput("normal", 1, VertexFormat::Float32x3);
	shader_1->addVertexInput("color", 2, VertexFormat::Float32x3);
	shader_1->addVertexInput("uv", 3, VertexFormat::Float32x2);

	shader_1->addVertexOutput("color", 0, VertexFormat::Float32x3);
	shader_1->addVertexOutput("normal", 1, VertexFormat::Float32x3);
	shader_1->addVertexOutput("uv", 2, VertexFormat::Float32x2);

	shader_1->addUniform("color");
	shader_1->addUniform("time"); 
    shader_1->addUniformMatrix("projection");
    shader_1->addUniformMatrix("view");
	shader_1->addUniformMatrix("model");

	shader_1->addTexture("baseColorTexture", textureView);
	shader_1->addTexture("metallicTexture", textureView);
	shader_1->addTexture("normalTexture", textureView);
	shader_1->addSampler("defaultSampler", defaultSampler);

	shader_1->build();
	
	vec3 focalPoint(0.0, 0.0, -2.0);
	float angle2 = 3.0f * PI / 4.0f;
	mat4x4 V(1.0);
	V = glm::translate(V, -focalPoint);
	V = glm::rotate(V, -angle2, vec3(1.0, 0.0, 0.0));
	shader_1->setUniform("view", V);

	float ratio = 640.0f / 480.0f;
	float focalLength = 2.0;
	float near = 0.01f;
	float far = 100.0f;
	float fov = 2 * glm::atan(1 / focalLength);
	mat4x4 proj = glm::perspective(fov, ratio, near, far);
	shader_1->setUniform("projection", proj);
	shader_1->setUniform("model", glm::mat4(1.0f));
	shader_1->setUniform("color", glm::vec4{ 1.0f,1.0f,1.0f,1.0f });

	Pass pass1(shader_1);
	pass1.addDepthBuffer(m_winWidth, m_winHeight, depthTextureFormat);
	

	Renderer renderer;
	renderer.addPass(pass1);

	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
		if (m_drag.active) {
			vec2 currentMouse = vec2((float)xpos, (float)ypos);
			vec2 delta = (currentMouse - m_drag.startMouse) * m_drag.sensitivity;
			m_cameraState.angles = m_drag.startCameraState.angles + delta;
			// Clamp to avoid going too far when orbitting up/down
			m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
			// Inertia
			m_drag.velocity = delta - m_drag.previousDelta;
			m_drag.previousDelta = delta;
		}
	});

	glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			switch (action) {
			case GLFW_PRESS:
				m_drag.active = true;
				double xpos, ypos;
				glfwGetCursorPos(window, &xpos, &ypos);
				m_drag.startMouse = vec2(-(float)xpos, (float)ypos);
				m_drag.startCameraState = m_cameraState;
				break;
			case GLFW_RELEASE:
				m_drag.active = false;
				break;
			}
		}
	});

	glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) {
		m_cameraState.zoom += m_drag.scrollSensitivity * static_cast<float>(yoffset);
		m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -2.0f, 2.0f);
	});

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
	 	updateViewMatrix(shader_1);
		renderer.draw();

	}

	Context::getInstance().shutdownGraphics();
	
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}