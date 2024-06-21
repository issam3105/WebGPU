#pragma once


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"
//#include "uniformsBuffer.h"
//#include "managers.h"
#include "shader.h"
#include "material.h"

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

			MaterialModule pbrMaterialModule = MaterialModuleManager::getInstance().getMaterialModule("pbrMat");
			material = new Material(pbrMaterialModule);
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

	struct Attribute
	{
		std::string name;
		Value value;
	};

	class Attributed {
	public:
		void addAttribute(std::string name, const Value& defaultValue) {
			m_attributes.push_back({ name , defaultValue });
			dirty = true;
		};

		Attribute& getAttribute(std::string name) {
			auto it = std::find_if(m_attributes.begin(), m_attributes.end(), [name](const Attribute& obj) {
				return obj.name == name;
			});
			assert(it != m_attributes.end());
			return *it;
		}

		void setAttribute(std::string name, const Value& value)
		{
			auto& attribute = getAttribute(name);
			attribute.value = value;
			dirty = true;
		}
		std::vector<Attribute>& getAttributes() { return m_attributes; }

	protected:
		std::vector<Attribute> m_attributes{};
		bool dirty = true;
	};


	class Scene : public Attributed{
	public:	
		Scene()
		{
		}

		void setNodes(std::vector<Issam::Node*> nodes) {
			std::vector<Issam::Node*> flatNodes;
			for (auto rootNode : nodes) {
				flattenNodes(rootNode, glm::mat4(1.0f), flatNodes);
			}
			m_nodes = flatNodes;
		}

		std::vector<Issam::Node*>& getNodes() { return m_nodes; }

		void updateUniforms(Shader* shader)
		{
			if (!dirty) return;
			for (auto& attrib : m_attributes)
			{
				if (shader->hasUniform(attrib.name))
					shader->setUniform(attrib.name, attrib.value, Shader::Binding::Scene);
			}
			dirty = false;
		}
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
		
	};
}