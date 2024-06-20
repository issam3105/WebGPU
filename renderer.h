#pragma once

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"
#include "node.h"



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
			auto pipeline = pass.getPipeline()->getRenderPipeline();
			renderPass.setPipeline(pipeline);
			renderPass.setBindGroup(2, m_scene->getBindGroup(pipeline.getBindGroupLayout(2)), 0, nullptr); //Scene uniforms
			for(auto& node : m_scene->getNodes())
			{
				Mesh* mesh = MeshManager::getInstance().get(node->meshId);
				if (mesh)
				{
					renderPass.setBindGroup(0, node->material->getBindGroup(pipeline.getBindGroupLayout(0)), 0, nullptr); //Material
					renderPass.setBindGroup(1, node->getBindGroup(pipeline.getBindGroupLayout(1)), 0, nullptr); //Node model
					renderPass.setVertexBuffer(0, mesh->getVertexBuffer()->getBuffer(), 0, mesh->getVertexBuffer()->getSize());
					if (mesh->getIndexBuffer() != nullptr)
					{
						renderPass.setIndexBuffer(mesh->getIndexBuffer()->getBuffer(), IndexFormat::Uint16, 0, mesh->getIndexBuffer()->getSize());
						renderPass.drawIndexed(mesh->getIndexBuffer()->getCount(), 1, 0, 0, 0);
					}
					else
						renderPass.draw(mesh->getVertexCount(), 1, 0, 0);
				}
			}

			// Build UI
			if (pass.getImGuiWrapper())
				pass.getImGuiWrapper()->draw(renderPass);

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

	void setScene(Issam::Scene* scene) { m_scene = scene; }
	//void setCamera(Issam::Camera* camera) { m_scene->camera = camera; }
	//Issam::Camera* getCamera() { return m_scene->camera; }
private:
	
	Queue m_queue{ nullptr };
	std::vector<Pass> m_passes;
	Issam::Scene* m_scene;
};