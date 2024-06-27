#pragma once

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"
#include "imgui_wrapper.h"

class Pass
{
public:
	Pass(Shader* shader) :
		m_shader(shader)
	{
	};
	~Pass() {
		m_depthBuffer.release();
		//depthTexture.destroy(); TODO
		//depthTexture.release();
		delete depthStencilAttachment;
		delete renderPassColorAttachment;
	};

	Shader* getShader() const { return m_shader; }
	void setImGuiWrapper(ImGUIWrapper* imGuiWrapper) { m_imGuiWrapper = imGuiWrapper; }
	ImGUIWrapper* getImGuiWrapper() { return m_imGuiWrapper; }

	void setDepthBuffer(TextureView bufferView)
	{
		m_depthBuffer = bufferView;
	}

	void setColorBuffer(TextureView bufferView)
	{
		m_colorBuffer = bufferView;
	}

	//const TextureView getDepthTextureView() const { return m_depthTextureView; }
	const RenderPassDepthStencilAttachment* getRenderPassDepthStencilAttachment() {
		if (m_depthBuffer)
		{
			depthStencilAttachment = new RenderPassDepthStencilAttachment();
			// The view of the depth texture
			depthStencilAttachment->view = m_depthBuffer;

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
		renderPassColorAttachment->view = m_colorBuffer ? m_colorBuffer : view;
		renderPassColorAttachment->resolveTarget = nullptr;
		renderPassColorAttachment->loadOp = m_clearColor ? LoadOp::Clear : LoadOp::Load;
		renderPassColorAttachment->storeOp = StoreOp::Store;
		renderPassColorAttachment->clearValue = m_clearColorValue;
		return renderPassColorAttachment;
	}

	void setPipeline(Pipeline* pipline) { m_pipline = pipline; }
	Pipeline* getPipeline() { return m_pipline; }

	void addFilter(std::string filter) { m_filters.push_back(filter); }
	const std::vector<std::string>& getFilters() { return m_filters; }

	void setClearColor(bool clear) { m_clearColor = clear; }
	void setClearColorValue(Color in_clearValue) { m_clearColorValue = in_clearValue; }

	enum class Type: uint8_t
	{
		SCENE = 0,
		FILTER
	};

	void setType(Type type) { m_type = type; }
	Type getType() { return m_type; }
private:
	RenderPassDepthStencilAttachment* depthStencilAttachment;
	RenderPassColorAttachment* renderPassColorAttachment;
	Shader* m_shader{ nullptr };
	Pipeline* m_pipline{ nullptr };
	TextureView m_depthBuffer{ nullptr };
	TextureView m_colorBuffer{ nullptr };
	ImGUIWrapper* m_imGuiWrapper{ nullptr };
	std::vector<std::string> m_filters;

	bool m_clearColor = true;
	Color m_clearColorValue{ 0.3, 0.3, 0.3, 1.0 };

	Type m_type{ Type::SCENE };
};