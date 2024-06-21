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

class Material : public Issam::Attributed {
public:
	Material()
	{
		auto& defaultSampler = SamplerManager().getInstance().getSampler("defaultSampler");
		auto& defaultTexture = TextureManager().getInstance().getTextureView("whiteTex");
		addAttribute("baseColorFactor", glm::vec4(1.0f));
		addAttribute("metallicFactor", 0.5f);
		addAttribute("roughnessFactor", 0.5f);

		addTexture("baseColorTexture", defaultTexture);
		addTexture("metallicRoughnessTexture", defaultTexture);
		addSampler("defaultSampler", defaultSampler);
		
	};
	~Material() = default;	
};
