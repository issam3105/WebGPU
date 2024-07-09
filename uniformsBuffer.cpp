#include "uniformsBuffer.h"
#include "context.h"

using namespace glm;


UniformsBuffer::UniformsBuffer()
{
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(UniformsData);
	// Make sure to flag the buffer as BufferUsage::Uniform
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	m_uniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

	Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(UniformsData));
};

void UniformsBuffer::setFloat(uint16_t offset, float value) {
	if (offset + 1 > UNIFORMS_MAX) {
		throw std::runtime_error("Uniforms buffer overflow");
	}
	m_uniformsData[offset] = value;
}

void UniformsBuffer::setVec4(uint16_t offset, const glm::vec4& value) {
	if (offset + 4 > UNIFORMS_MAX) {
		throw std::runtime_error("Uniforms buffer overflow");
	}
	m_uniformsData[offset] = value.x;
	m_uniformsData[offset + 1] = value.y;
	m_uniformsData[offset + 2] = value.z;
	m_uniformsData[offset + 3] = value.w;
}

void UniformsBuffer::setMat4(uint16_t offset, const glm::mat4& mat) {
	if (offset + 16 > UNIFORMS_MAX) {
		throw std::runtime_error("Uniforms buffer overflow");
	}
	for (int i = 0; i < 16; ++i) {
		m_uniformsData[offset + i] = mat[i / 4][i % 4];
	}
}

void UniformsBuffer::set(uint16_t handle, const UniformValue& value)
{
	if (std::holds_alternative< glm::vec4>(value))
	{
		setVec4(handle, std::get<glm::vec4>(value));
	}
	else if (std::holds_alternative< float>(value))
	{
		float newValue = std::get<float>(value);
		setVec4(handle, glm::vec4(newValue, newValue, newValue, newValue)); //avoid padding
	}
	else if (std::holds_alternative< glm::mat4x4>(value))
	{
		setMat4(handle, std::get<mat4x4>(value));
	}
	else
		assert(false);
	Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(UniformsData)); //TODO envoyer que la partie modifie
}
