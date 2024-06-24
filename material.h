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

//class MaterialModel : public Issam::Attributed {
//public:
//	MaterialModel()
//	{	
//	};
//	~MaterialModel() = default;
//};




class Material : public Issam::Attributed
{
public:
	Material(const std::string&  materialAttributesId)
	{
		auto materialModel = Issam::AttributedManager::getInstance().get(materialAttributesId);
		m_attributes = materialModel.getAttributes();
		m_textures = materialModel.getTextures();
		m_samplers = materialModel.getSamplers();
	};
	~Material() = default;
};



