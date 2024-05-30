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

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

using namespace wgpu;

#ifdef WEBGPU_BACKEND_WGPU
TextureFormat swapChainFormat = surface.getPreferredFormat(adapter);
#else
TextureFormat swapChainFormat = TextureFormat::BGRA8Unorm;
#endif

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

		std::cout << "Requesting adapter..." << std::endl;
		m_surface = glfwGetWGPUSurface(m_instance, window);
		RequestAdapterOptions adapterOpts;
		adapterOpts.compatibleSurface = m_surface;
		m_adapter = m_instance.requestAdapter(adapterOpts);
		std::cout << "Got adapter: " << m_adapter << std::endl;

		std::cout << "Requesting device..." << std::endl;
		DeviceDescriptor deviceDesc;
		deviceDesc.label = "My Device";
		deviceDesc.requiredFeaturesCount = 0;
		deviceDesc.requiredLimits = nullptr;
		deviceDesc.defaultQueue.label = "The default queue";
		m_device = m_adapter.requestDevice(deviceDesc);
		std::cout << "Got device: " << m_device << std::endl;

		// Add an error callback for more debug info
		auto h = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
			std::cout << "Device error: type " << type;
			if (message) std::cout << " (message: " << message << ")";
			std::cout << std::endl;
		});



		std::cout << "Creating swapchain..." << std::endl;

		SwapChainDescriptor swapChainDesc;
		swapChainDesc.width = 640;
		swapChainDesc.height = 480;
		swapChainDesc.usage = TextureUsage::RenderAttachment;
		swapChainDesc.format = swapChainFormat;
		swapChainDesc.presentMode = PresentMode::Fifo;
		m_swapChain = m_device.createSwapChain(m_surface, swapChainDesc);
		std::cout << "Swapchain: " << m_swapChain << std::endl;
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
		std::string shaderSource(size, ' ');
		file.seekg(0);
		file.read(shaderSource.data(), size);

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

		// Setup the actual payload of the shader code descriptor
		shaderCodeDesc.code = shaderSource.c_str();

		m_shaderModule = Context::getInstance().getDevice().createShaderModule(shaderDesc);
		std::cout << "Shader module: " << m_shaderModule << std::endl;
	};

	~Shader() { m_shaderModule.release(); }

	VertexState getVertexState() const
	{
		VertexState vertexState;
		vertexState.module = m_shaderModule;
		vertexState.entryPoint = "vs_main";
		vertexState.constantCount = 0;
		vertexState.constants = nullptr;
		return vertexState;
	}

	FragmentState getFragmentState() const
	{
		FragmentState fragmentState;
		fragmentState.module = m_shaderModule;
		fragmentState.entryPoint = "fs_main";
		fragmentState.constantCount = 0;
		fragmentState.constants = nullptr;
		return fragmentState;
	}
	 
	//ShaderModule getShaderModule() { return m_shaderModule; }
	
private:
	ShaderModule m_shaderModule{ nullptr };
};

//https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/hello-triangle.html#render-pipeline
class Pipeline
{
public:
	Pipeline(const Shader shader) {
		//Creating render pipeline
		RenderPipelineDescriptor pipelineDesc;

		// Vertex fetch
		// (We don't use any input buffer so far)
		pipelineDesc.vertex.bufferCount = 0;
		pipelineDesc.vertex.buffers = nullptr;

		// Vertex shader
		pipelineDesc.vertex = shader.getVertexState();


		// Primitive assembly and rasterization
		pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
		pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
		pipelineDesc.primitive.frontFace = FrontFace::CCW;
		pipelineDesc.primitive.cullMode = CullMode::None;

		// Fragment shader
		FragmentState fragmentState = shader.getFragmentState();
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
		pipelineDesc.depthStencil = nullptr;

		// Multi-sampling
		// Samples per pixel
		pipelineDesc.multisample.count = 1;
		// Default value for the mask, meaning "all bits on"
		pipelineDesc.multisample.mask = ~0u;
		// Default value as well (irrelevant for count = 1 anyways)
		pipelineDesc.multisample.alphaToCoverageEnabled = false;

		// Pipeline layout
		pipelineDesc.layout = nullptr;

		m_pipeline = Context::getInstance().getDevice().createRenderPipeline(pipelineDesc);
		std::cout << "Render pipeline: " << m_pipeline << std::endl;
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
	Pass(Pipeline* pipeline): 
		m_pipeline(pipeline)
	{
	};
	~Pass() = default;

//	RenderPassDescriptor getRenderPassDescriptor() const { return m_renderPassDesc; }
	const Pipeline* getPipeline() const { return m_pipeline; }
private:
//	RenderPassDescriptor m_renderPassDesc;
//	RenderPassColorAttachment m_renderPassColorAttachment;
	Pipeline* m_pipeline { nullptr };
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
			RenderPassDescriptor m_renderPassDesc;
			RenderPassColorAttachment m_renderPassColorAttachment;
			m_renderPassColorAttachment.view = nextTexture;
			m_renderPassColorAttachment.resolveTarget = nullptr;
			m_renderPassColorAttachment.loadOp = LoadOp::Clear;
			m_renderPassColorAttachment.storeOp = StoreOp::Store;
			m_renderPassColorAttachment.clearValue = Color{ 0.9, 0.1, 0.2, 1.0 };
			m_renderPassDesc.colorAttachmentCount = 1;
			m_renderPassDesc.colorAttachments = &m_renderPassColorAttachment;

			m_renderPassDesc.depthStencilAttachment = nullptr;
			m_renderPassDesc.timestampWriteCount = 0;
			m_renderPassDesc.timestampWrites = nullptr;
			
			RenderPassEncoder renderPass = encoder.beginRenderPass(m_renderPassDesc);

			// In its overall outline, drawing a triangle is as simple as this:
			// Select which render pipeline to use
			renderPass.setPipeline(pass.getPipeline()->getRenderPipeline());
			// Draw 1 instance of a 3-vertices shape
			renderPass.draw(3, 1, 0, 0);

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

	Shader shader_1("C:/Dev/WebGPU/build/Debug/data/sahder_1.wgsl");
	
	Pipeline* pipeline = new Pipeline(shader_1);

	Pass pass1(pipeline);

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