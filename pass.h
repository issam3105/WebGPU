#pragma once

#pragma once
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"

class Pass
{
public:
	Pass(Shader* shader) :
		m_shader(shader)
	{
		//	m_shader->build();
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
	const RenderPassDepthStencilAttachment* getRenderPassDepthStencilAttachment() {
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
	Shader* m_shader{ nullptr };
	TextureView m_depthTextureView{ nullptr };
};