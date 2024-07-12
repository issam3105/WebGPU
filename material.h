#pragma once

using namespace wgpu;

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
using namespace glm;

#include "uniformsBuffer.h"
#include "attributed.h"
#include "managers.h"


class Material 
{
public:
	Material()
	{
		auto& groups = Issam::AttributedManager::getInstance().getAll(Issam::Binding::Material);
		for (auto& [groupName, attributeGroup]: groups)
		{
			m_attributeds[groupName] = new Issam::AttributedRuntime(groupName, attributeGroup.getVersionCount());
		}
		
	};
	~Material() = default;

	void setAttribute(const std::string& name, const Issam::AttributeValue& value)
	{
		for (auto attributed : m_attributeds)
		{
			if (attributed.second->hasAttribute(name))
				attributed.second->setAttribute(name, value);
		}
	}

	void setAttribute(const std::string& materialAttributesId, const std::string& name, const  Issam::AttributeValue& value, size_t version = 0)
	{
		m_attributeds[materialAttributesId]->setAttribute(name, value, version);
	}

	Issam::Attribute& getAttribute(const std::string& name)
	{
		for (auto attributed : m_attributeds)
		{
			if (attributed.second->hasAttribute(name))
				return attributed.second->getAttribute(name);
		}
	}

	UniformValue& getUniform(const std::string& name)
	{
		for (auto attributed : m_attributeds)
		{
			if (attributed.second->hasAttribute(name))
			{
				assert(std::holds_alternative< UniformValue>(attributed.second->getAttribute(name).value)); 
				return  std::get<UniformValue>(attributed.second->getAttribute(name).value);
			}
		}
	}

	Issam::AttributedRuntime* getAttibutedRuntime(const std::string& attributedId)
	{
		return m_attributeds[attributedId];
	}

private:
	std::unordered_map<std::string, Issam::AttributedRuntime*> m_attributeds{};
};



