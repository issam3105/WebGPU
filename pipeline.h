#pragma once
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"
#include "shader.h"

using namespace wgpu;

class Pipeline
{
public:
	enum class BlendingMode : uint8_t
	{
		Replace = 0,
		Over,
		Multiply,
		Custom
	};


	Pipeline(Shader* shader, TextureFormat swapChainFormat, TextureFormat depthTextureFormat, BlendingMode blendingMode) {
		//Creating render pipeline
		RenderPipelineDescriptor pipelineDesc;

		// Vertex fetch
		std::vector<VertexAttribute> m_vertexAttribs;
		//Positions
		VertexAttribute vertexAttribute;
		vertexAttribute.shaderLocation = 0;
		vertexAttribute.format = VertexFormat::Float32x3;
		vertexAttribute.offset = 0;
		m_vertexAttribs.push_back(vertexAttribute);
		//Normals
		vertexAttribute.shaderLocation = 1;
		vertexAttribute.format = VertexFormat::Float32x3;
		vertexAttribute.offset = 3 * sizeof(float);
		m_vertexAttribs.push_back(vertexAttribute);
		//Tangents
		vertexAttribute.shaderLocation = 2;
		vertexAttribute.format = VertexFormat::Float32x3;
		vertexAttribute.offset = 6 * sizeof(float);
		m_vertexAttribs.push_back(vertexAttribute);
		//TexCoords
		vertexAttribute.shaderLocation = 3;
		vertexAttribute.format = VertexFormat::Float32x2;
		vertexAttribute.offset = 9 * sizeof(float);
		m_vertexAttribs.push_back(vertexAttribute);

		VertexBufferLayout vertexBufferLayout;
		// [...] Build vertex buffer layout
		vertexBufferLayout.attributeCount = (uint32_t)m_vertexAttribs.size();
		vertexBufferLayout.attributes = m_vertexAttribs.data();
		// == Common to attributes from the same buffer ==
		vertexBufferLayout.arrayStride = 11 * sizeof(float);
		vertexBufferLayout.stepMode = VertexStepMode::Vertex;
		
		VertexState vertexState;
		vertexState.module = shader->getShaderModule();
		vertexState.entryPoint = "vs_main";
		vertexState.constantCount = 0;
		vertexState.constants = nullptr;

		pipelineDesc.vertex = vertexState;
		pipelineDesc.vertex.bufferCount = 1;
		pipelineDesc.vertex.buffers = &vertexBufferLayout;

		// Primitive assembly and rasterization
		pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
		pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
		pipelineDesc.primitive.frontFace = FrontFace::CCW;
		pipelineDesc.primitive.cullMode = CullMode::None;

		// Fragment shader
		FragmentState fragmentState;
		fragmentState.module = shader->getShaderModule();
		fragmentState.entryPoint = "fs_main";
		fragmentState.constantCount = 0;
		fragmentState.constants = nullptr;
		pipelineDesc.fragment = &fragmentState;

		ColorTargetState colorTarget;
		colorTarget.format = swapChainFormat;
		colorTarget.blend = &getBlendState(blendingMode);
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
		layoutDesc.bindGroupLayoutCount = shader->getBindGroupLayouts().size();
		layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)shader->getBindGroupLayouts().data();
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

	BlendState getBlendState(BlendingMode blendingMode)
	{
		// Configure blend state
		BlendState blendState;

		switch (blendingMode)
		{
		case Pipeline::BlendingMode::Replace:
			blendState.color.srcFactor = BlendFactor::One;
			blendState.color.dstFactor = BlendFactor::Zero;
			blendState.color.operation = BlendOperation::Add;
			// We leave the target alpha untouched:
			blendState.alpha.srcFactor = BlendFactor::One;
			blendState.alpha.dstFactor = BlendFactor::Zero;
			blendState.alpha.operation = BlendOperation::Add;
			break;
		case Pipeline::BlendingMode::Over:
			// Usual alpha blending for the color:
			blendState.color.srcFactor = BlendFactor::SrcAlpha;
			blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
			blendState.color.operation = BlendOperation::Add;
			// We leave the target alpha untouched:
			blendState.alpha.srcFactor = BlendFactor::One;
			blendState.alpha.dstFactor = BlendFactor::OneMinusSrcAlpha;
			blendState.alpha.operation = BlendOperation::Add;
			break;
		case Pipeline::BlendingMode::Multiply:
			blendState.color.srcFactor = BlendFactor::Zero;
			blendState.color.dstFactor = BlendFactor::SrcAlpha;
			blendState.color.operation = BlendOperation::Add;
			// We leave the target alpha untouched:
			blendState.alpha.srcFactor = BlendFactor::Zero;
			blendState.alpha.dstFactor = BlendFactor::SrcAlpha;
			blendState.alpha.operation = BlendOperation::Add;
			break;
		case Pipeline::BlendingMode::Custom: assert(false);
			break;
		default: assert(false);
			break;
		}
		return blendState;
	}
};