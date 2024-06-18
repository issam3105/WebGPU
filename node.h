#pragma once


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"
#include "uniformsBuffer.h"
#include "managers.h"

#include <array>


using namespace glm;

namespace Issam {

	struct NodeProperties {
		glm::mat4 transform{ glm::mat4(1) };
	};

	class Node {
	public:
		Node() {
			BufferDescriptor bufferDesc;
			bufferDesc.label = name.c_str();
			bufferDesc.mappedAtCreation = false;
			bufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
			bufferDesc.size = sizeof(NodeProperties);
			nodeUniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);
			Context::getInstance().getDevice().getQueue().writeBuffer(nodeUniformBuffer, 0, &nodeProperties, sizeof(NodeProperties));

			//TextureView whiteTextureView = nullptr;
			//Texture  whiteTexture = Issam::createWhiteTexture(&whiteTextureView);
			////TextureManager().getInstance().add("whiteTex", whiteTextureView);

			//// Create a sampler
			//Sampler defaultSampler = Issam::createDefaultSampler();

			material = new Material();
			material->addUniform("baseColorFactor", glm::vec4(1.0f));
			material->addUniform("time", 0.0f);

			material->addTexture("baseColorTexture", TextureManager().getInstance().getTextureView("whiteTex"));
			material->addSampler("defaultSampler", SamplerManager().getInstance().getSampler("defaultSampler"));
		};


		BindGroup getBindGroup(BindGroupLayout bindGroupLayout) {
			if (!dirtyBindGroup)
				return bindGroup;
			// Bind Group
			std::vector<BindGroupEntry> bindGroupEntries(1, Default);
			bindGroupEntries[0].binding = 0;
			bindGroupEntries[0].buffer = nodeUniformBuffer;
			bindGroupEntries[0].size = sizeof(NodeProperties);

			BindGroupDescriptor bindGroupDesc;
			bindGroupDesc.label = name.c_str();
			bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
			bindGroupDesc.entries = bindGroupEntries.data();
			bindGroupDesc.layout = bindGroupLayout;
			bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
			dirtyBindGroup = false;
			return bindGroup;
		}

		Node(const Node& other) {
			name = other.name;
			children = other.children;
			meshId = other.meshId;
			material = other.material;
			nodeUniformBuffer = other.nodeUniformBuffer;
		}

		void setTransform(glm::mat4 in_transform) { 
			nodeProperties.transform = in_transform;
			updateUniformBuffer();
		}
		glm::mat4& getTransform() {
			return nodeProperties.transform;
		}
		std::string name;
		std::vector<Node*> children;
		std::string meshId;

		Material* material;

	private:
		void updateUniformBuffer() { Context::getInstance().getDevice().getQueue().writeBuffer(nodeUniformBuffer, 0, &nodeProperties, sizeof(NodeProperties)); }

		NodeProperties nodeProperties;
		Buffer nodeUniformBuffer{ nullptr };
		BindGroup bindGroup{ nullptr };
		bool dirtyBindGroup = true;

	};

	struct CameraProperties
	{
		glm::mat4 view{ glm::mat4(1.0) };
		glm::mat4 projection{ glm::mat4(1.0) };
	};

	class Camera {
	public:
		Camera() {
			BufferDescriptor bufferDesc;
			bufferDesc.label = "camera";
			bufferDesc.mappedAtCreation = false;
			bufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
			bufferDesc.size = sizeof(CameraProperties);
			cameraUniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);
			Context::getInstance().getDevice().getQueue().writeBuffer(cameraUniformBuffer, 0, &cameraProperties, sizeof(CameraProperties));
		}
		void setView(glm::mat4 in_view) { 
			cameraProperties.view = in_view; 
		    updateUniformBuffer();
		}
		void setProjection(glm::mat4 in_projection) { 
			cameraProperties.projection = in_projection;
			updateUniformBuffer();
		}	

		BindGroup getBindGroup(BindGroupLayout bindGroupLayout) {
			if (!dirtyBindGroup)
				return bindGroup;
			// Bind Group
			std::vector<BindGroupEntry> bindGroupEntries(1, Default);
			bindGroupEntries[0].binding = 0;
			bindGroupEntries[0].buffer = cameraUniformBuffer;
			bindGroupEntries[0].size = sizeof(CameraProperties);

			BindGroupDescriptor bindGroupDesc;
			bindGroupDesc.label = "camera";
			bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
			bindGroupDesc.entries = bindGroupEntries.data();
			bindGroupDesc.layout = bindGroupLayout;
			bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
			dirtyBindGroup = false;
			return bindGroup;
		}
	private:
		void updateUniformBuffer() { Context::getInstance().getDevice().getQueue().writeBuffer(cameraUniformBuffer, 0, &cameraProperties, sizeof(CameraProperties)); }
		CameraProperties cameraProperties;
		Buffer cameraUniformBuffer{ nullptr };
		BindGroup bindGroup{ nullptr };
		bool dirtyBindGroup = true;
	};

	class Scene {
	public:	
		void setNodes(std::vector<Issam::Node*> nodes) {
			std::vector<Issam::Node*> flatNodes;
			for (auto rootNode : nodes) {
				flattenNodes(rootNode, glm::mat4(1.0f), flatNodes);
			}
			m_nodes = flatNodes;
		}

		void setCamera(Camera* in_camera)
		{
			m_camera = in_camera;
		}

		Issam::Camera* getCamera() { return m_camera; }
		std::vector<Issam::Node*>& getNodes() { return m_nodes; }
	private:
		void flattenNodes(Issam::Node* node, glm::mat4 parentTransform, std::vector<Issam::Node*>& flatNodes) {
			glm::mat4 globalTransform = parentTransform * node->getTransform();
			Issam::Node* flatNode = new Node(*node); // I need a copy
			flatNode->setTransform(globalTransform);
			flatNode->children.clear();
			flatNodes.push_back(flatNode);

			for (auto child : node->children) {
				flattenNodes(child, globalTransform, flatNodes);
			}
		}

		std::vector<Issam::Node*> m_nodes;
		Issam::Camera* m_camera;
	};
}