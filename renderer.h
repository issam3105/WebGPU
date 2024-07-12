#pragma once


#include "context.h"
#include "scene.h"


class Renderer
{
public:
	Renderer()
	{
		m_queue = Context::getInstance().getDevice().GetQueue();

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
		CommandEncoder encoder = Context::getInstance().getDevice().CreateCommandEncoder(&commandEncoderDesc);

		TextureView nextTexture = Context::getInstance().getSwapChain().GetCurrentTextureView();
		if (!nextTexture) {
			std::cerr << "Cannot acquire next swap chain texture" << std::endl;
		}

		int passIdx = 0;
		auto view = m_scene->getRegistry().view<const Issam::WorldTransform, Issam::Filters, Issam::MeshRenderer>();
		for (auto& pass : m_passes)
		{
			RenderPassDescriptor renderPassDesc;
			renderPassDesc.label = ("pass_" + std::to_string(passIdx++)).c_str();

			renderPassDesc.colorAttachmentCount = 1;
			renderPassDesc.colorAttachments = pass->getRenderPassColorAttachment(nextTexture);//&renderPassColorAttachment;
			renderPassDesc.depthStencilAttachment = pass->getRenderPassDepthStencilAttachment();

			renderPassDesc.timestampWrites = nullptr;

			RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);

			if (pass->getType() == Pass::Type::SCENE)
			{
				const auto pipeline = pass->getPipeline()->getRenderPipeline();
				renderPass.SetPipeline(pipeline);
				Shader* shader = pass->getShader();
				auto& layouts = shader->getBindGroupLayouts();

				auto& attribSceneId = shader->getAttributedId(Issam::Binding::Scene);
				uint32_t dynamicOffsetScene = pass->getUniformBufferVersion(Issam::Binding::Scene) * sizeof(UniformsData);
				size_t dynamicOffsetCountScene = m_scene->getAttibutedRuntime(attribSceneId)->getNumVersions() - 1;
				renderPass.SetBindGroup(2, m_scene->getAttibutedRuntime(attribSceneId)->getBindGroup(layouts[static_cast<int>(Issam::Binding::Scene)]), 0, nullptr); //Scene uniforms

				
				for (auto entity : view) 
				{
					const Issam::Filters& filters = view.get<Issam::Filters>(entity);
					bool shouldDraw = std::any_of(filters.filters.begin(), filters.filters.end(),
						[&pass](const std::string& filter) {
						return std::find(pass->getFilters().begin(), pass->getFilters().end(), filter) != pass->getFilters().end();
					});
					if (!shouldDraw) continue;

					const Issam::MeshRenderer& meshRenderer = view.get<Issam::MeshRenderer>(entity);
					auto transform = view.get<Issam::WorldTransform>(entity);
					Mesh* mesh = MeshManager::getInstance().get(meshRenderer.meshId);
					if (mesh)
					{
						Material* material = meshRenderer.material;

						auto& attribMaterialId = shader->getAttributedId(Issam::Binding::Material);
						uint32_t dynamicOffsetMaterial = pass->getUniformBufferVersion(Issam::Binding::Material) * sizeof(UniformsData);
						size_t dynamicOffsetCountMaterial = material->getAttibutedRuntime(attribMaterialId)->getNumVersions() - 1;
						renderPass.SetBindGroup(0, material->getAttibutedRuntime(attribMaterialId)->getBindGroup(layouts[static_cast<int>(Issam::Binding::Material)]), dynamicOffsetCountMaterial, &dynamicOffsetMaterial); //Material

						auto& attribNodelId = shader->getAttributedId(Issam::Binding::Node);
						uint32_t dynamicOffsetNode = pass->getUniformBufferVersion(Issam::Binding::Node) * sizeof(UniformsData);
						size_t dynamicOffsetCountNode = transform.getAttibutedRuntime(attribNodelId)->getNumVersions() - 1;
						renderPass.SetBindGroup(1, transform.getAttibutedRuntime(attribNodelId)->getBindGroup(layouts[static_cast<int>(Issam::Binding::Node)]), dynamicOffsetCountNode, &dynamicOffsetNode); //Node model
						
						renderPass.SetVertexBuffer(0, mesh->getVertexBuffer()->getBuffer(), 0, mesh->getVertexBuffer()->getSize());
						if (mesh->getIndexBuffer() != nullptr)
						{
							renderPass.SetIndexBuffer(mesh->getIndexBuffer()->getBuffer(), IndexFormat::Uint16, 0, mesh->getIndexBuffer()->getSize());
							renderPass.DrawIndexed(mesh->getIndexBuffer()->getCount(), 1, 0, 0, 0);
						}
						else
							renderPass.Draw(mesh->getVertexCount(), 1, 0, 0);
					}
				}
			}
			else if (pass->getType() == Pass::Type::FILTER)
			{
				const auto pipeline = pass->getPipeline()->getRenderPipeline();
				renderPass.SetPipeline(pipeline);
				Shader* shader = pass->getShader();
				auto& layouts = shader->getBindGroupLayouts();
				auto& attribSceneId = shader->getAttributedId(Issam::Binding::Scene);
				renderPass.SetBindGroup(0, m_scene->getAttibutedRuntime(attribSceneId)->getBindGroup(layouts[0]), 0, nullptr); //TODO layouts[0] ?
				if (fullScreenMesh)
				{
					renderPass.SetVertexBuffer(0, fullScreenMesh->getVertexBuffer()->getBuffer(), 0, fullScreenMesh->getVertexBuffer()->getSize());
					renderPass.Draw(fullScreenMesh->getVertexCount(), 1, 0, 0);
				}
			}
			else
			{
				pass->getWrapper()->draw(renderPass);
			}

				

			//renderPass.popDebugGroup();

			renderPass.End();
		}
		Context::getInstance().getDevice().Tick();

		CommandBufferDescriptor cmdBufferDescriptor;
		cmdBufferDescriptor.label = "Command buffer";
		CommandBuffer command = encoder.Finish(&cmdBufferDescriptor);
		
		std::vector<CommandBuffer> commands;
		commands.push_back(command);
		m_queue.Submit(commands.size(), commands.data());

		Context::getInstance().getSwapChain().Present();
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