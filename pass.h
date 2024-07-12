#pragma once


#include "context.h"
#include "imgui_wrapper.h"

class Pass
{
public:
	Pass()
	{
		m_renderPassColorAttachment = new RenderPassColorAttachment();
		m_depthStencilAttachment = new RenderPassDepthStencilAttachment();
	};
	~Pass() {
	//	m_depthBuffer.release();
		//depthTexture.destroy(); TODO
		//depthTexture.release();
		delete m_depthStencilAttachment;
		delete m_renderPassColorAttachment;
	};

	void setShader(Shader* shader) { m_shader = shader; }
	Shader* getShader() const { return m_shader; }
	void setWrapper(Wrapper* wrapper) { m_wrapper = wrapper; }
	Wrapper* getWrapper() { return m_wrapper; }

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
			// The view of the depth texture
			m_depthStencilAttachment->view = m_depthBuffer;

			// The initial value of the depth buffer, meaning "far"
			m_depthStencilAttachment->depthClearValue = 1.0f;
			// Operation settings comparable to the color attachment
			m_depthStencilAttachment->depthLoadOp = LoadOp::Clear;
			m_depthStencilAttachment->depthStoreOp = StoreOp::Store;
			// we could turn off writing to the depth buffer globally here
			m_depthStencilAttachment->depthReadOnly = false;

			// Stencil setup, mandatory but unused
			m_depthStencilAttachment->stencilClearValue = 0;

			if (!m_useStencil)
			{
				m_depthStencilAttachment->stencilLoadOp = LoadOp::Undefined;
				m_depthStencilAttachment->stencilStoreOp = StoreOp::Undefined;
			}
			else
			{
				m_depthStencilAttachment->stencilLoadOp = LoadOp::Clear;
				m_depthStencilAttachment->stencilStoreOp = StoreOp::Store;
			}

			m_depthStencilAttachment->stencilReadOnly = m_useStencil ? false : true;
			return m_depthStencilAttachment;
		}
		else
		{
			return nullptr;
		}
	}

	const RenderPassColorAttachment* getRenderPassColorAttachment(TextureView view) {
		m_renderPassColorAttachment->view = m_colorBuffer ? m_colorBuffer : view;
		m_renderPassColorAttachment->resolveTarget = nullptr;
		m_renderPassColorAttachment->loadOp = m_clearColor ? LoadOp::Clear : LoadOp::Load;
		m_renderPassColorAttachment->storeOp = StoreOp::Store;
		m_renderPassColorAttachment->clearValue = m_clearColorValue;
		return m_renderPassColorAttachment;
	}

	void setPipeline(Pipeline* pipline) { m_pipline = pipline; }
	const Pipeline* getPipeline() const { return m_pipline; }

	void addFilter(std::string filter) { m_filters.push_back(filter); }
	const std::vector<std::string>& getFilters() { return m_filters; }

	void setClearColor(bool clear) { m_clearColor = clear; }
	void setClearColorValue(Color in_clearValue) { m_clearColorValue = in_clearValue; }
	void setUseStencil(bool active = true) { m_useStencil = active; }
	enum class Type: uint8_t
	{
		SCENE = 0,
		FILTER, 
		CUSTUM
	};

	void setType(Type type) { m_type = type; }
	Type getType() { return m_type; }

	void setUniformBufferVersion(Issam::Binding binding, size_t uniformBufferVersion) { m_uniformBufferVersion[binding] = uniformBufferVersion; }
	const size_t& getUniformBufferVersion(Issam::Binding binding) {
		if (m_uniformBufferVersion.find(binding) != m_uniformBufferVersion.end())
			return m_uniformBufferVersion[binding];
		else
			return 0;
	}

private:
	
	RenderPassDepthStencilAttachment* m_depthStencilAttachment;
	RenderPassColorAttachment* m_renderPassColorAttachment;
	Shader* m_shader{ nullptr };
	Pipeline* m_pipline{ nullptr };
	TextureView m_depthBuffer{ nullptr };
	TextureView m_colorBuffer{ nullptr };
	Wrapper* m_wrapper{ nullptr };
	std::vector<std::string> m_filters;

	bool m_clearColor = true;
	Color m_clearColorValue{ 0.3, 0.3, 0.3, 1.0 };
	bool m_useStencil = false;
	Type m_type{ Type::SCENE };
	//size_t m_uniformBufferVersion = 0;
	std::unordered_map<Issam::Binding, size_t>m_uniformBufferVersion;
};