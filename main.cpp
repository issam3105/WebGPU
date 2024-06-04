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

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <array>
#include <variant>

using namespace wgpu;
namespace fs = std::filesystem;

using glm::mat4x4;
using glm::vec4;
using glm::vec3;

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

		//Requesting device...
		DeviceDescriptor deviceDesc;
		deviceDesc.label = "My Device";
		deviceDesc.requiredFeaturesCount = 0;
		deviceDesc.requiredLimits = nullptr;
		deviceDesc.defaultQueue.label = "The default queue";
		m_device = m_adapter.requestDevice(deviceDesc);
		assert(m_device);

		// Add an error callback for more debug info
		auto h = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
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
	int index;
	Value value;
	bool isMatrix() const { return std::holds_alternative< glm::mat4>(value); }
	bool isFloat() const { return std::holds_alternative< float>(value); }
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
	}
};

class Uniforms
{	
public:
	Uniforms()
	{
		BufferDescriptor bufferDesc;
		bufferDesc.size = sizeof(std::array<glm::vec4, UNIFORMS_MAX>);
		// Make sure to flag the buffer as BufferUsage::Uniform
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
		bufferDesc.mappedAtCreation = false;
		m_uniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

		Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(std::array<vec4, UNIFORMS_MAX>));

		BindGroupLayoutEntry bindingLayout = Default;
		// The binding index as used in the @binding attribute in the shader
		bindingLayout.binding = 0;
		// The stage that needs to access this resource
		bindingLayout.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
		bindingLayout.buffer.type = BufferBindingType::Uniform;
		bindingLayout.buffer.minBindingSize = sizeof(std::array<vec4, UNIFORMS_MAX>);

		// Create a bind group layout
		BindGroupLayoutDescriptor bindGroupLayoutDesc{};
		bindGroupLayoutDesc.entryCount = 1;
		bindGroupLayoutDesc.entries = &bindingLayout;
		m_bindGroupLayout = Context::getInstance().getDevice().createBindGroupLayout(bindGroupLayoutDesc);

		// Create a binding
		BindGroupEntry binding{};
		// The index of the binding (the entries in bindGroupDesc can be in any order)
		binding.binding = 0;
		// The buffer it is actually bound to
		binding.buffer = m_uniformBuffer;
		// We can specify an offset within the buffer, so that a single buffer can hold
		// multiple uniform blocks.
		binding.offset = 0;
		// And we specify again the size of the buffer.
		binding.size = sizeof(std::array<vec4, UNIFORMS_MAX>);

		// A bind group contains one or multiple bindings
		BindGroupDescriptor bindGroupDesc;
		bindGroupDesc.layout = m_bindGroupLayout;
		// There must be as many bindings as declared in the layout!
		bindGroupDesc.entryCount = bindGroupLayoutDesc.entryCount;
		bindGroupDesc.entries = &binding;
		m_bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
	};
	~Uniforms() = default;

	void addUniform(std::string name) {
		assert(m_uniformIndex < UNIFORMS_MAX);
		m_uniforms.push_back({ name, m_uniformIndex++, glm::vec4(0.0f)});
	};

	void addUniformMatrix(std::string name) {
		assert(m_uniformIndex + 4 < UNIFORMS_MAX);
		m_uniforms.push_back({ name, m_uniformIndex, glm::mat4x4(1.0f) });
		m_uniformIndex += 4;
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
		if (uniform.isVec4())
			m_uniformsData[uniform.index] = uniform.getVec4();
		else if (uniform.isFloat())
		{
			float newValue = uniform.getFloat();
			m_uniformsData[uniform.index] = glm::vec4(newValue, newValue, newValue, newValue);
		}
		else if (uniform.isMatrix())
		{
			int index = getUniform(name).index;
			mat4x4 mat = uniform.getMat4x4();
			m_uniformsData[index]     = { mat[0][0], mat[0][1], mat[0][2], mat[0][3] };
			m_uniformsData[index + 1] = { mat[1][0], mat[1][1], mat[1][2], mat[1][3] };
			m_uniformsData[index + 2] = { mat[2][0], mat[2][1], mat[2][2], mat[2][3] };
			m_uniformsData[index + 3] = { mat[3][0], mat[3][1], mat[3][2], mat[3][3] };
		}
		else
			assert(false);
		Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(std::array<vec4, UNIFORMS_MAX>)); //TODO envoyer que la partie modifie
	}

	BindGroupLayout getBindGroupLayout() { return m_bindGroupLayout; }
	BindGroup getBindGroup() {return m_bindGroup;}
	
	std::vector<Uniform> getUniforms() { return m_uniforms;}

private:
	
	std::vector<Uniform> m_uniforms{};
	std::array<vec4, UNIFORMS_MAX> m_uniformsData {};
	uint16_t m_uniformIndex = 0;
	
	Buffer m_uniformBuffer{ nullptr };
	BindGroupLayout m_bindGroupLayout{ nullptr };
	BindGroup m_bindGroup{ nullptr };
};

class Shader
{
public:
	Shader(const std::filesystem::path& path) {
		std::ifstream file(path);
		if (!file.is_open()) {
			std::cerr << path.c_str() << " not found ! " << std::endl;
		}
		file.seekg(0, std::ios::end);
		size_t size = file.tellg();
		m_shaderSource = std::string(size, ' ');
		file.seekg(0);
		file.read(m_shaderSource.data(), size);

	};

	~Shader() { 
		m_shaderModule.release();
		delete m_uniforms;
	}

	VertexState getVertexState() 
	{
		VertexState vertexState;
		vertexState.module = getShaderModule();
		vertexState.entryPoint = "vs_main";
		vertexState.constantCount = 0;
		vertexState.constants = nullptr;
		return vertexState;
	}

	FragmentState getFragmentState() 
	{
		FragmentState fragmentState;
		fragmentState.module = getShaderModule();
		fragmentState.entryPoint = "fs_main";
		fragmentState.constantCount = 0;
		fragmentState.constants = nullptr;
		return fragmentState;
	}
	
	Uniforms* getUniforms() { return m_uniforms; }

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

private:
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
	ShaderModule getShaderModule()
	{
		if (m_shaderModule) return m_shaderModule;
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
			vertexInputOutputStr += "    @location(" + std::to_string(vertexInput.location) + ") " + vertexInput.name + ": "+ toString(vertexInput.format) +", \n";
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
		for (const auto& uniform : m_uniforms->getUniforms())
		{
			vertexUniformsStr += "    " + uniform.name + ": " + (uniform.isMatrix() ? "mat4x4f" : "vec4f") + ", \n";
		}
		vertexUniformsStr += "}; \n \n";
		vertexUniformsStr += "@group(0) @binding(0) var<uniform> u_uniforms: Uniforms;\n"; //TODO prametrique

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
		return m_shaderModule;
	}
	ShaderModule m_shaderModule{ nullptr };
	Uniforms* m_uniforms{ new Uniforms() }; 
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
		layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&shader->getUniforms()->getBindGroupLayout();
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

	const TextureView getDepthTextureView() const { return m_depthTextureView; }
private:

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

		for (const auto& pass : m_passes)
		{
			RenderPassDescriptor renderPassDesc;
			RenderPassColorAttachment renderPassColorAttachment;
			renderPassColorAttachment.view = nextTexture;
			renderPassColorAttachment.resolveTarget = nullptr;
			renderPassColorAttachment.loadOp = LoadOp::Clear;
			renderPassColorAttachment.storeOp = StoreOp::Store;
			renderPassColorAttachment.clearValue = Color{ 0.3, 0.3, 0.3, 1.0 };
			renderPassDesc.colorAttachmentCount = 1;
			renderPassDesc.colorAttachments = &renderPassColorAttachment;

			if (pass.getDepthTextureView())
			{
				RenderPassDepthStencilAttachment depthStencilAttachment;
				// The view of the depth texture
				depthStencilAttachment.view = pass.getDepthTextureView();

				// The initial value of the depth buffer, meaning "far"
				depthStencilAttachment.depthClearValue = 1.0f;
				// Operation settings comparable to the color attachment
				depthStencilAttachment.depthLoadOp = LoadOp::Clear;
				depthStencilAttachment.depthStoreOp = StoreOp::Store;
				// we could turn off writing to the depth buffer globally here
				depthStencilAttachment.depthReadOnly = false;

				// Stencil setup, mandatory but unused
				depthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
				depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
				depthStencilAttachment.stencilStoreOp = StoreOp::Store;
#else
				depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
				depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
#endif
				depthStencilAttachment.stencilReadOnly = true;

				renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
			}
			else
			{
				renderPassDesc.depthStencilAttachment = nullptr;
			}

			renderPassDesc.timestampWriteCount = 0;
			renderPassDesc.timestampWrites = nullptr;
			
			RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
		
			for (auto [meshId , mesh] : MeshManager::getInstance().getAll())
			{
				Pipeline* pipeline = new Pipeline(pass.getShader(), mesh->getVertexBufferLayouts()); //TODO le creer le moins possible
				renderPass.setPipeline(pipeline->getRenderPipeline());
				
				pass.getShader()->getUniforms()->setUniform("time", static_cast<float>(glfwGetTime()));

				float angle1 = static_cast<float>(glfwGetTime());
				mat4x4 S = glm::scale(mat4x4(1.0), vec3(0.3f));
				mat4x4 T1 = glm::translate(mat4x4(1.0), vec3(0.5, 0.0, 0.0));
				mat4x4 R1 = glm::rotate(mat4x4(1.0), angle1, vec3(0.0, 0.0, 1.0));
				mat4x4 modelMatrix = R1 * T1 * S;
				pass.getShader()->getUniforms()->setUniform("model", modelMatrix);

				renderPass.setBindGroup(0, pass.getShader()->getUniforms()->getBindGroup(), 0, nullptr);
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
			
			renderPass.end();
			renderPass.release();
		}

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
		}
	}

	int indexCount = static_cast<int>(vertexData.size());
	int vertexCount = static_cast<int>(vertexData.size() );
	auto* data = vertexData.data();
	VertexBuffer* vertexBuffer = new VertexBuffer(vertexData.data(), vertexData.size() *sizeof(VertexAttributes), vertexCount);
	vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x3, 0);
	vertexBuffer->addVertexAttrib("Normal", 1, VertexFormat::Float32x3, 3 * sizeof(float));
	vertexBuffer->addVertexAttrib("Color", 2, VertexFormat::Float32x3, 6 * sizeof(float)); // offset = sizeof(Position)
	vertexBuffer->setAttribsStride(9 * sizeof(float));

	
	Mesh* mesh = new Mesh();
	mesh->addVertexBuffer(vertexBuffer);
	MeshManager::getInstance().add("obj_1", mesh);

	return true;
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
	loadGeometryFromObj(DATA_DIR  "/pyramid.obj");
    
	Shader* shader_1 = new Shader(DATA_DIR  "/sahder_1.wgsl");
	shader_1->addVertexInput("position", 0, VertexFormat::Float32x3);
	shader_1->addVertexInput("normal", 1, VertexFormat::Float32x3);
	shader_1->addVertexInput("color", 2, VertexFormat::Float32x3);
	shader_1->addVertexOutput("color", 0, VertexFormat::Float32x3);
	shader_1->addVertexOutput("normal", 1, VertexFormat::Float32x3);

	shader_1->getUniforms()->addUniform("color");
	shader_1->getUniforms()->addUniform("time"); 
    shader_1->getUniforms()->addUniformMatrix("projection");
    shader_1->getUniforms()->addUniformMatrix("view");
	shader_1->getUniforms()->addUniformMatrix("model");
	
	vec3 focalPoint(0.0, 0.0, -2.0);
	float angle2 = 3.0f * PI / 4.0f;
	mat4x4 V(1.0);
	V = glm::translate(V, -focalPoint);
	V = glm::rotate(V, -angle2, vec3(1.0, 0.0, 0.0));
	shader_1->getUniforms()->setUniform("view", V);

	float ratio = 640.0f / 480.0f;
	float focalLength = 2.0;
	float near = 0.01f;
	float far = 100.0f;
	float fov = 2 * glm::atan(1 / focalLength);
	mat4x4 proj = glm::perspective(fov, ratio, near, far);
	shader_1->getUniforms()->setUniform("projection", proj);
	shader_1->getUniforms()->setUniform("color", glm::vec4{ 0.8f,0.2f,0.8f,1.0f });

	Pass pass1(shader_1);
	pass1.addDepthBuffer(m_winWidth, m_winHeight, depthTextureFormat);
	

	Renderer renderer;
	renderer.addPass(pass1);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		renderer.draw();

	}

	Context::getInstance().shutdownGraphics();
	
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}