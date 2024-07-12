#pragma once

#include <variant>
#include <array>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include<string>
#include<vector>


#define UNIFORMS_MAX 64

#include <webgpu/webgpu_cpp.h>
using namespace wgpu;

using UniformValue = std::variant<float, glm::vec4, glm::mat4>;
using UniformsData = std::array<float, UNIFORMS_MAX>;


struct Uniform {
	std::string name;
	int handle;
	UniformValue value;
	bool isMatrix() const { return std::holds_alternative< glm::mat4>(value); }
};

class UniformsBuffer
{
public:
	UniformsBuffer(size_t numVersions = 1);
	
	~UniformsBuffer() = default;

	template<typename T>
	uint16_t allocate()
	{
		uint16_t handle = m_uniformOffset;
		m_uniformOffset += sizeof(T) / sizeof(float);
		assert(m_uniformOffset < UNIFORMS_MAX);
		return handle;
	};

	void set(uint16_t handle, const UniformValue& value, size_t version = 0);

	Buffer getBuffer() { return m_uniformBuffer; }

private:
	void setFloat(uint16_t offset, float value, size_t version);
	void setVec4(uint16_t offset, const glm::vec4& value, size_t version);
	void setMat4(uint16_t offset, const glm::mat4& value, size_t version);

	//UniformsData m_uniformsData{};
	std::vector<UniformsData> m_uniformsData;
	uint16_t m_uniformOffset = 0;
	size_t m_numVersions = 1;
	Buffer m_uniformBuffer{ nullptr };
};