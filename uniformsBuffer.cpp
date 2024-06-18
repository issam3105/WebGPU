#include "uniformsBuffer.h"
#include "context.h"

using namespace glm;


UniformsBuffer::UniformsBuffer()
{
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(std::array<glm::vec4, UNIFORMS_MAX>);
	// Make sure to flag the buffer as BufferUsage::Uniform
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_uniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

	Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(std::array<vec4, UNIFORMS_MAX>));
};

uint16_t UniformsBuffer::allocateVec4() {
	assert(m_uniformIndex < UNIFORMS_MAX);
	return m_uniformIndex++;
}

uint16_t UniformsBuffer::allocateMat4()
{
	assert(m_uniformIndex + 4 < UNIFORMS_MAX);
	uint16_t currentHandle = m_uniformIndex;
	m_uniformIndex += 4;
	return currentHandle;
}

void UniformsBuffer::set(uint16_t handle, const Value& value)
{
	if (std::holds_alternative< glm::vec4>(value))
		m_uniformsData[handle] = std::get<glm::vec4>(value);
	else if (std::holds_alternative< float>(value))
	{
		float newValue = std::get<float>(value);
		m_uniformsData[handle] = glm::vec4(newValue, newValue, newValue, newValue);
	}
	else if (std::holds_alternative< glm::mat4x4>(value))
	{

		mat4x4 mat = std::get<mat4x4>(value);
		m_uniformsData[handle] = { mat[0][0], mat[0][1], mat[0][2], mat[0][3] };
		m_uniformsData[handle + 1] = { mat[1][0], mat[1][1], mat[1][2], mat[1][3] };
		m_uniformsData[handle + 2] = { mat[2][0], mat[2][1], mat[2][2], mat[2][3] };
		m_uniformsData[handle + 3] = { mat[3][0], mat[3][1], mat[3][2], mat[3][3] };
	}
	else
		assert(false);
	Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(std::array<vec4, UNIFORMS_MAX>)); //TODO envoyer que la partie modifie
}
