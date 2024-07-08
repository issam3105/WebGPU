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
	const std::string c_backgroundSceneAttributes = "backgroundSceneModel";
	const std::string c_diltationSceneAttributes = "dilatationSceneModel";

	//Components
	struct Name {
		std::string name;
	};

	struct LocalTransform
	{
		glm::mat4 m_matrix = glm::mat4(1.0);
	};

	class WorldTransform
	{
	public:
		WorldTransform() {
			m_attributes = new AttributedRuntime(Issam::c_pbrNodeAttributes);
			m_attributes->setAttribute("model", m_matrix);
		}
		WorldTransform(glm::mat4 transform): m_matrix( transform)
		{
			m_attributes = new AttributedRuntime(Issam::c_pbrNodeAttributes);
			m_attributes->setAttribute("model", m_matrix);
		}
		~WorldTransform() { /*delete m_attributes;*/ };
		void setTransform(glm::mat4 transform) { 
			m_matrix = transform;
			m_attributes->setAttribute("model", m_matrix);
		}
		const glm::mat4& getTransform() const{ return m_matrix; }
		BindGroup getBindGroup(BindGroupLayout bindGroupLayout) { return m_attributes->getBindGroup(bindGroupLayout); }
	private:
		glm::mat4 m_matrix = glm::mat4(1.0);
		AttributedRuntime* m_attributes;
	};

	struct Filters {
	public:
		void add(std::string filter) { filters.push_back(filter); }
		void remove(std::string filter) {
			auto it = std::find(filters.begin(), filters.end(), filter);
			filters.erase(it);
		}
	public:
		std::vector<std::string> filters;
	};

	struct MeshRenderer {
		std::string meshId{};
		Material* material = nullptr;

		~MeshRenderer() {
			//TODO Fuite m�moire !!
			//delete material;
			//MeshManager::getInstance().remove(meshId);
		}
	};

	struct Hierarchy {
		entt::entity parent = entt::null;
		std::vector<entt::entity> children;
	};

	struct Camera {
		glm::vec3 m_pos = glm::vec3(0.0);
		glm::mat4 m_view = glm::mat4(1.0);
		glm::mat4 m_projection = glm::mat4(1.0);
	};

	struct Light {
		glm::vec3 m_direction = glm::vec3(0.0);
	};

	

	class Scene 
	{
	public:	
		

		Scene()
		{
			m_attributeds[Issam::c_pbrSceneAttributes] = new AttributedRuntime(Issam::c_pbrSceneAttributes);
			m_attributeds[Issam::c_unlitSceneAttributes] = new AttributedRuntime(Issam::c_unlitSceneAttributes);
			m_attributeds[Issam::c_backgroundSceneAttributes] = new AttributedRuntime(Issam::c_backgroundSceneAttributes);
			m_attributeds[Issam::c_diltationSceneAttributes] = new AttributedRuntime(Issam::c_diltationSceneAttributes);

			m_registry.on_update<Hierarchy>().connect<&Scene::updateWorldTransforms>(*this);
			m_registry.on_update<LocalTransform>().connect<&Scene::updateWorldTransforms>(*this);

			m_registry.on_update<Camera>().connect<&Scene::onCameraModified>(*this);
			m_registry.on_update<Light>().connect<&Scene::onLightModified>(*this);
		}

		void setAttribute(const std::string& name, const Value& value)
		{
			for (auto attributed : m_attributeds)
			{
				if (attributed.second->hasAttribute(name))
					attributed.second->setAttribute(name, value);
			}
		}

		Attribute& getAttribute(const std::string& name)
		{
			for (auto attributed : m_attributeds)
			{
				if (attributed.second->hasAttribute(name))
					return attributed.second->getAttribute(name);
			}
		}

		Issam::AttributedRuntime* getAttibutedRuntime(const std::string& attributedId)
		{
			return m_attributeds[attributedId];
		}

		entt::entity addEntity(const glm::mat4& localTransformMatrix = glm::mat4(1.0f))
		{
			const auto entity = m_registry.create();
			m_entities.push_back(entity);
			LocalTransform localTransform;
			localTransform.m_matrix = localTransformMatrix;
			m_registry.emplace<LocalTransform>(entity, localTransform);
			m_registry.emplace<Hierarchy>(entity, Hierarchy());
			
			assert(m_registry.valid(entity));
			//updateWorldTransforms();
			return entity;
		}

		entt::entity addCamera()
		{
			const auto entity = m_registry.create();
			m_entities.push_back(entity);
			m_registry.emplace<Camera>(entity, Camera());
			return entity;
		}

		entt::entity addLight()
		{
			const auto entity = m_registry.create();
			m_entities.push_back(entity);
			m_registry.emplace<Light>(entity, Light());
			return entity;
		}

		void setLocalTransform(entt::entity entity, const glm::mat4& in_localTransform)
		{
			auto& localTransform = m_registry.get<LocalTransform>(entity);
			localTransform.m_matrix = in_localTransform;
			m_registry.patch<LocalTransform>(entity);
		}

		void setMeshRenderer(entt::entity entity,const MeshRenderer& meshRenderer)
		{
			assert(m_registry.valid(entity));
			m_registry.emplace<MeshRenderer>(entity, meshRenderer);
		}

		void setFilters(entt::entity entity, const Filters& filters)
		{
			assert(m_registry.valid(entity));
			m_registry.emplace<Filters>(entity, filters);
		}

		void setName(entt::entity entity, const std::string& name)
		{
			m_registry.emplace<Name>(entity, Name({ name }));
		}

		void addChild(entt::entity parent, entt::entity child) {
			auto& parentHierarchy = m_registry.get<Hierarchy>(parent);
			parentHierarchy.children.push_back(child);

			auto& childHierarchy = m_registry.get<Hierarchy>(child);
			childHierarchy.parent = parent;

			m_registry.patch<Hierarchy>(parent);
			//updateWorldTransforms();
		}

		void removeChild(entt::entity parent, entt::entity child) {
			auto& parentHierarchy = m_registry.get<Hierarchy>(parent);
			parentHierarchy.children.erase(
				std::remove(parentHierarchy.children.begin(), parentHierarchy.children.end(), child),
				parentHierarchy.children.end()
			);

			auto& childHierarchy = m_registry.get<Hierarchy>(child);
			childHierarchy.parent = entt::null;

			m_registry.patch<Hierarchy>(parent);
		}

		void removeEntity(entt::entity entity)
		{
			m_registry.destroy(entity);
		}

		void clear()
		{
			m_registry.clear();
		}

		entt::registry& getRegistry() { return m_registry; };

		void printHierarchy(entt::entity entity, int level = 0) {
			const auto& hierarchy = m_registry.get<Hierarchy>(entity);
			const auto& name = m_registry.get<Name>(entity);
			const auto& localTransform = m_registry.get<LocalTransform>(entity);

			std::cout << std::string(level * 2, '-') << name.name << std::endl;
			//std::cout << std::string(level * 2, ' ')  << mat4_to_string(localTransform.m_transform, std::string(level * 2, ' ')) << std::endl;

			for (auto child : hierarchy.children) {
				printHierarchy(child, level + 1);
			}
		}

	private:
		//Slots
		void updateWorldTransforms(entt::registry& registry, entt::entity entity)
		{
			calculateWorldTransforms(entity);
		}

		void onCameraModified(entt::registry& registry, entt::entity entity) {
			const auto& camera = registry.get<Camera>(entity);
			setAttribute("cameraPosition", vec4(camera.m_pos, 0.0));
			setAttribute("view", camera.m_view);
			setAttribute("projection", camera.m_projection);
		}

		void onLightModified(entt::registry& registry, entt::entity entity) {
			const auto& light = registry.get<Light>(entity);
			setAttribute("lightDirection", vec4(light.m_direction, 0.0));
		}

		void calculateWorldTransforms(entt::entity entity, const glm::mat4& parentTransform = glm::mat4(1.0f)) {
			auto& localTransform = m_registry.get<LocalTransform>(entity);
			auto& globalTransform = m_registry.get_or_emplace<WorldTransform>(entity);

			globalTransform.setTransform(parentTransform * localTransform.m_matrix);

			const auto& hierarchy = m_registry.get<Hierarchy>(entity);
			for (auto child : hierarchy.children) {
				calculateWorldTransforms(child, globalTransform.getTransform());
			}
		}

		std::string mat4_to_string(const glm::mat4& mat, std::string espace) {
			std::ostringstream oss;
			oss << espace << "["
				<< mat[0][0] << ", " << mat[0][1] << ", " << mat[0][2] << ", " << mat[0][3] << "\n "
				<< espace << mat[1][0] << ", " << mat[1][1] << ", " << mat[1][2] << ", " << mat[1][3] << "\n"
				<< espace << mat[2][0] << ", " << mat[2][1] << ", " << mat[2][2] << ", " << mat[2][3] << "\n "
				<< espace << mat[3][0] << ", " << mat[3][1] << ", " << mat[3][2] << ", " << mat[3][3]
				<< "]";
			return oss.str();
		}

		

		entt::registry m_registry;
		std::unordered_map<std::string, Issam::AttributedRuntime*> m_attributeds{};
		std::vector<entt::entity> m_entities;
	};
}