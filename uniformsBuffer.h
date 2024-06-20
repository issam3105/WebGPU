#pragma once

#include <variant>
#include <array>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>


#include <webgpu/webgpu.hpp>

#define UNIFORMS_MAX 15

using namespace wgpu;

using Value = std::variant<float, glm::vec4, glm::mat4>;
using UniformsData = std::array<glm::vec4, UNIFORMS_MAX>;

class UniformsBuffer
{
public:
	UniformsBuffer();
	~UniformsBuffer() = default;

	uint16_t allocateVec4();

	uint16_t allocateMat4();

	void set(uint16_t handle, const Value& value);

	Buffer getBuffer() { return m_uniformBuffer; }

private:
	UniformsData m_uniformsData{};
	uint16_t m_uniformIndex = 0;
	Buffer m_uniformBuffer{ nullptr };
};