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
	const std::string c_pbrSceneAttributes = "pbrSceneAttributes";
	const std::string c_pbrNodeAttributes = "pbrNodeAttributes";
	const std::string c_unlitSceneAttributes = "unlitSceneAttributes";

	class Node : public AttributedRuntime{
	public:
		Node() {
			setAttributes(c_pbrNodeAttributes);
			/*auto materialModel = Issam::AttributedManager::getInstance().get(c_pbrNodeAttributes);
			m_attributes = materialModel.getAttributes();
			m_textures = materialModel.getTextures();
			m_samplers = materialModel.getSamplers();*/
			//addAttribute("model", glm::mat4(1.0));
			auto* pbrMaterial = new Material(c_pbrMaterialAttributes);
			auto* unlitMaterial = new Material(c_unlitMaterialAttributes);
			unlitMaterial->setAttribute("colorFactor", glm::vec4(1.0, 0.5, 0.0, 1.0));
			materials.push_back(pbrMaterial);
			materials.push_back(unlitMaterial);

			m_filters.push_back("pbr");
		};


		Node(const Node& other) {
			name = other.name;
			children = other.children;
			meshId = other.meshId;
			materials = other.materials;
			m_attributes = other.m_attributes;
			//m_textures = other.m_textures;
			//m_samplers = other.m_samplers;

			m_filters = other.m_filters;
		}

		void setTransform(glm::mat4 in_transform) { 
			setAttribute("model", in_transform);
		}
		glm::mat4& getTransform() {
			return std::get<glm::mat4>( getAttribute("model").value);
		}

		void addFilter(std::string filter) { m_filters.push_back(filter); }
		void removeFilter(std::string filter) {
			auto it = std::find(m_filters.begin(), m_filters.end(), filter);
			m_filters.erase(it); 
		}

		const std::vector<std::string>& getFilters() { return m_filters; }

		std::string name;
		std::vector<Node*> children;
		std::string meshId;

		std::vector<Material*> materials;

	private:

		std::vector<std::string> m_filters;
	};


	/*class SceneModel : public Attributed
	{
	public:
		SceneModel() {

		};
		~SceneModel() {};
	};*/

	/*class SceneModelManager
	{
	public:
		SceneModelManager() = default;
		~SceneModelManager() = default;

		static SceneModelManager& getInstance() {
			static SceneModelManager sceneModelManager;
			return sceneModelManager;
		};

		bool add(const std::string& id, SceneModel SceneModel) {
			m_sceneModels[id] = SceneModel;
			return true;
		}
		SceneModel get(const std::string& id) {
			return  m_sceneModels[id];
		}
		void clear()
		{
			m_sceneModels.clear();
		}
		std::unordered_map<std::string, SceneModel>& getAll() { return m_sceneModels; }
	private:
		std::unordered_map<std::string, SceneModel> m_sceneModels{};
	};
	*/

	class Scene : public AttributedRuntime
	{
	public:	
		Scene()
		{
			setAttributes(Issam::c_pbrSceneAttributes);
			//auto sceneModel = Issam::AttributedManager::getInstance().get(Issam::c_pbrSceneAttributes);
			//m_attributes = sceneModel.getAttributes();
			//m_textures = sceneModel.getTextures();
			//m_samplers = sceneModel.getSamplers();
			/*addAttribute("view", mat4(1.0));
			addAttribute("projection", mat4(1.0));
			addAttribute("cameraPosition", vec4(0.0));
			addAttribute("lightDirection", vec4(1.0));*/
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