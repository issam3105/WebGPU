#pragma once
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"
#include "shader.h"

using namespace wgpu;

class Pipeline
{
public:
	Pipeline(Shader* shader, std::vector<VertexBufferLayout> vertexBufferLayouts, TextureFormat swapChainFormat, TextureFormat depthTextureFormat) {
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