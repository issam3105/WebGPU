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
		assert(m_shaderModule);
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
	
private:
	ShaderModule m_shaderModule{ nullptr };
	
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
	~Pass() = default;

	Shader* getShader() const { return m_shader; }
private:

	Shader* m_shader { nullptr };
};

class VertexBuffer
{
public:
	VertexBuffer(std::vector<float> vertexData, int vertexCount):
		m_data (vertexData.data()),
		m_count(vertexCount)
	{
		// Create vertex buffer
		m_size = vertexData.size() * sizeof(float);
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
	float* m_data;
	uint64_t  m_size;
	int m_count{ 0 };
	std::vector<VertexAttribute> m_vertexAttribs;
	uint64_t m_attribsStride = 0;

	friend class Mesh;
};

class IndexBuffer
{
public:
	IndexBuffer(std::vector<uint16_t> indexData, int indexCount) :
		m_data(indexData.data()),
		m_count(indexCount)
	{
		m_size = indexData.size() * sizeof(uint16_t);
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
	const uint64_t getSize() const { return m_size; }
	const int getCount() const { return m_count; }

private:
	Buffer m_buffer{ nullptr };
	uint16_t* m_data;
	uint64_t  m_size;
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
		
			for (auto [meshId , mesh] : MeshManager::getInstance().getAll())
			{
				Pipeline* pipeline = new Pipeline(pass.getShader(), mesh->getVertexBufferLayouts()); //TODO le creer le moins possible
				renderPass.setPipeline(pipeline->getRenderPipeline());
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
	VertexBuffer* vertexBufferPos = new VertexBuffer(positionData, vertexCount);
	vertexBufferPos->addVertexAttrib("Position", 0, VertexFormat::Float32x2, 0);
	vertexBufferPos->setAttribsStride(2 * sizeof(float));

	VertexBuffer* vertexBufferColor = new VertexBuffer(colorData, vertexCount);
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

	//VertexBuffer* vertexBuffer = new VertexBuffer(vertexData, vertexCount);
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

	VertexBuffer* vertexBuffer = new VertexBuffer(pointData, vertexCount);
	vertexBuffer->addVertexAttrib("Position", 0, VertexFormat::Float32x2, 0);
	vertexBuffer->addVertexAttrib("Color", 1, VertexFormat::Float32x3, 2 * sizeof(float)); // offset = sizeof(Position)
	vertexBuffer->setAttribsStride(5 * sizeof(float));

	IndexBuffer* indexBuffer = new IndexBuffer(indexData, indexCount);
	Mesh* mesh = new Mesh();
	mesh->addVertexBuffer(vertexBuffer);
	mesh->addIndexBuffer(indexBuffer);
	MeshManager::getInstance().add("ColoredPlane", mesh);
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
	addColoredPlane();
    
	Shader* shader_1 = new Shader(DATA_DIR  "/sahder_1.wgsl");

	Pass pass1(shader_1);

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