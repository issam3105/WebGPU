#pragma once

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"
#include "scene.h"


class Renderer
{
public:
	Renderer()
	{
		m_queue = Context::getInstance().getDevice().getQueue();

		std::vector<Vertex> vertices;
		Vertex vertex;
		vertex.position = glm::vec3(-1.0, -1.0, 0.0);
		vertex.uv = glm::vec2(0.0, 0.0);
		vertices.push_back(vertex);
		vertex.position = glm::vec3(1.0, -1.0, 0.0);
		vertex.uv = glm::vec2(1.0, 0.0);
		vertices.push_back(vertex);
		vertex.position = glm::vec3(-1.0, 1.0, 0.0);
		vertex.uv = glm::vec2(0.0, 1.0);
		vertices.push_back(vertex);
		vertex.position = glm::vec3(1.0, 1.0, 0.0);
		vertex.uv = glm::vec2(1.0, 1.0);
		vertices.push_back(vertex);
		vertex.position = glm::vec3(-1.0, 1.0, 0.0);
		vertex.uv = glm::vec2(0.0, 1.0);
		vertices.push_back(vertex);
		vertex.position = glm::vec3(1.0, -1.0, 0.0);
		vertex.uv = glm::vec2(1.0, 0.0);
		vertices.push_back(vertex);

		fullScreenMesh = new Mesh();
		fullScreenMesh->setVertices(vertices);
	};
	~Renderer() {
		delete fullScreenMesh;
	};

	void draw()
	{
		CommandEncoderDescriptor commandEncoderDesc;
		commandEncoderDesc.label = "Command Encoder";
		CommandEncoder encoder = Context::getInstance().getDevice().createCommandEncoder(commandEncoderDesc);

		TextureView nextTexture = Context::getInstance().getSwapChain().getCurrentTextureView();
		if (!nextTexture) {
			std::cerr << "Cannot acquire next swap chain texture" << std::endl;
		}

		int passIdx = 0;
		for (auto& pass : m_passes)
		{
			RenderPassDescriptor renderPassDesc;
			renderPassDesc.label = ("pass_" + std::to_string(passIdx++)).c_str();

			renderPassDesc.colorAttachmentCount = 1;
			renderPassDesc.colorAttachments = pass->getRenderPassColorAttachment(nextTexture);//&renderPassColorAttachment;
			renderPassDesc.depthStencilAttachment = pass->getRenderPassDepthStencilAttachment();

			renderPassDesc.timestampWriteCount = 0;
			renderPassDesc.timestampWrites = nullptr;

			RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

			if (pass->getType() == Pass::Type::SCENE)
			{
				const auto pipeline = pass->getPipeline()->getRenderPipeline();
				renderPass.setPipeline(pipeline);
				Shader* shader = pass->getShader();
				auto& layouts = shader->getBindGroupLayouts();
				auto& attribSceneId = shader->getAttributedId(Issam::Binding::Scene);
				renderPass.setBindGroup(2, m_scene->getAttibutedRuntime(attribSceneId)->getBindGroup(layouts[static_cast<int>(Issam::Binding::Scene)]), 0, nullptr); //Scene uniforms
				auto view = m_scene->getRegistry().view<const Issam::WorldTransform, Issam::Filters, Issam::MeshRenderer, Issam::Hierarchy>();
				for (auto entity : view) 
				{
					Issam::Filters filters = view.get<Issam::Filters>(entity);
					bool shouldDraw = std::any_of(filters.filters.begin(), filters.filters.end(),
						[&pass](const std::string& filter) {
						return std::find(pass->getFilters().begin(), pass->getFilters().end(), filter) != pass->getFilters().end();
					});
					if (!shouldDraw) continue;

					Issam::MeshRenderer meshRenderer = view.get<Issam::MeshRenderer>(entity);
					auto transform = view.get<Issam::WorldTransform>(entity);
					Mesh* mesh = MeshManager::getInstance().get(meshRenderer.meshId);
					if (mesh)
					{
						Material* material = meshRenderer.material;
						//Issam::AttributedRuntime* nodeAttributes = transform.getAttributedRuntime();
						auto& attribMaterialId = shader->getAttributedId(Issam::Binding::Material);
						auto& attribNodelId = shader->getAttributedId(Issam::Binding::Node);
						renderPass.setBindGroup(0, material->getAttibutedRuntime(attribMaterialId)->getBindGroup(layouts[static_cast<int>(Issam::Binding::Material)]), 0, nullptr); //Material
						renderPass.setBindGroup(1, transform.getAttibutedRuntime(attribNodelId)->getBindGroup(layouts[static_cast<int>(Issam::Binding::Node)]), 0, nullptr); //Node model
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
			}
			else if (pass->getType() == Pass::Type::FILTER)
			{
				const auto pipeline = pass->getPipeline()->getRenderPipeline();
				renderPass.setPipeline(pipeline);
				Shader* shader = pass->getShader();
				auto& layouts = shader->getBindGroupLayouts();
				auto& attribSceneId = shader->getAttributedId(Issam::Binding::Scene);
				renderPass.setBindGroup(0, m_scene->getAttibutedRuntime(attribSceneId)->getBindGroup(layouts[0]), 0, nullptr); //TODO layouts[0] ?
				if (fullScreenMesh)
				{
					renderPass.setVertexBuffer(0, fullScreenMesh->getVertexBuffer()->getBuffer(), 0, fullScreenMesh->getVertexBuffer()->getSize());
					renderPass.draw(fullScreenMesh->getVertexCount(), 1, 0, 0);
				}
			}
			else
			{
				pass->getWrapper()->draw(renderPass);
			}

				

			//renderPass.popDebugGroup();

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

	void addPass(Pass* pass)
	{
		m_passes.push_back(pass);
	}

	void setScene(Issam::Scene* scene) {
		m_scene = scene;
		
	}
	//void setCamera(Issam::Camera* camera) { m_scene->camera = camera; }
	//Issam::Camera* getCamera() { return m_scene->camera; }
private:
	
	Queue m_queue{ nullptr };
	std::vector<Pass*> m_passes;
	Issam::Scene* m_scene;

	Mesh* fullScreenMesh{ nullptr };
};