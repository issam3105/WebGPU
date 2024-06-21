#pragma once


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"

#include "shader.h"
#include "material.h"
#include "attributed.h"

#include <array>


using namespace glm;

namespace Issam {
	class Node : public Attributed{
	public:
		Node() {
			addAttribute("model", glm::mat4(1.0));
			material = new Material();
		};


		Node(const Node& other) {
			name = other.name;
			children = other.children;
			meshId = other.meshId;
			material = other.material;
			m_attributes = other.m_attributes;
		}

		void setTransform(glm::mat4 in_transform) { 
			setAttribute("model", in_transform);
		}
		glm::mat4& getTransform() {
			return std::get<glm::mat4>( getAttribute("model").value);
		}


		std::string name;
		std::vector<Node*> children;
		std::string meshId;

		Material* material;

	private:


	};

	

	class Scene : public Attributed
	{
	public:	
		Scene()
		{
			addAttribute("view", mat4(1.0));
			addAttribute("projection", mat4(1.0));
			addAttribute("cameraPosition", vec4(0.0));
			addAttribute("lightDirection", vec4(1.0));
		}

		void setNodes(std::vector<Issam::Node*> nodes) {
			std::vector<Issam::Node*> flatNodes;
			for (auto rootNode : nodes) {
				flattenNodes(rootNode, glm::mat4(1.0f), flatNodes);
			}
			m_nodes = flatNodes;
		}

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
	};
}