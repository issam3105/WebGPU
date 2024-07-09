#pragma once

#include <webgpu/webgpu.hpp>
using namespace wgpu;

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
using namespace glm;

#include "uniformsBuffer.h"
#include "attributed.h"
#include "managers.h"

const std::string c_pbrMaterialAttributes = "pbrMaterialModel";
const std::string c_unlitMaterialAttributes = "unlitMaterialModel";
const std::string c_unlit2MaterialAttributes = "unlit2MaterialModel"; //TODO remove ?

//class MaterialModel : public Issam::Attributed {
//public:
//	MaterialModel()
//	{	
//	};
//	~MaterialModel() = default;
//};




class Material 
{
public:
	Material()
	{
		m_attributeds[c_pbrMaterialAttributes] = new Issam::AttributedRuntime(c_pbrMaterialAttributes);
		m_attributeds[c_unlitMaterialAttributes] = new Issam::AttributedRuntime(c_unlitMaterialAttributes);
		m_attributeds[c_unlit2MaterialAttributes] = new Issam::AttributedRuntime(c_unlit2MaterialAttributes);
		
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

	void setAttribute(const std::string& materialAttributesId, const std::string& name, const  Issam::AttributeValue& value)
	{
		m_attributeds[materialAttributesId]->setAttribute(name, value);
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



