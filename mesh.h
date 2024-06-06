#pragma once

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "context.h"

class VertexBuffer
{
public:
	VertexBuffer(void* vertexData, size_t size, int vertexCount) :
		m_data(vertexData),
		m_count(vertexCount)
	{
		// Create vertex buffer
		m_size = size;
		BufferDescriptor bufferDesc;
		bufferDesc.size = m_size;
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
		bufferDesc.mappedAtCreation = false;
		m_buffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

		// Upload geometry data to the buffer
		Context::getInstance().getDevice().getQueue().writeBuffer(m_buffer, 0, m_data, bufferDesc.size);
	};

	~VertexBuffer() {};

	void addVertexAttrib(const std::string& attribName, int location, VertexFormat format, int offset)
	{
		VertexAttribute vertexAttribute;
		vertexAttribute.shaderLocation = location;
		vertexAttribute.format = format;
		vertexAttribute.offset = offset;
		m_vertexAttribs.push_back(vertexAttribute);
	}
	void setAttribsStride(uint64_t attribsStride) { m_attribsStride = attribsStride; }


	const Buffer& getBuffer()const { return m_buffer; }
	const uint64_t getSize() const { return m_size; }
	const int getCount() const { return m_count; }

private:
	Buffer m_buffer{ nullptr };
	void* m_data;
	size_t  m_size;
	int m_count{ 0 };
	std::vector<VertexAttribute> m_vertexAttribs;
	uint64_t m_attribsStride = 0;

	friend class Mesh;
};

class IndexBuffer
{
public:
	IndexBuffer(void* indexData, size_t size, int indexCount) :
		m_data(indexData),
		m_count(indexCount)
	{
		m_size = size * sizeof(uint16_t);
		BufferDescriptor bufferDesc;
		bufferDesc.size = m_size;
		bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;

		bufferDesc.size = (bufferDesc.size + 3) & ~3; // round up to the next multiple of 4
		m_buffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

		// Upload geometry data to the buffer
		Context::getInstance().getDevice().getQueue().writeBuffer(m_buffer, 0, m_data, bufferDesc.size);
	};
	~IndexBuffer() {};

	const Buffer& getBuffer()const { return m_buffer; }
	const size_t getSize() const { return m_size; }
	const int getCount() const { return m_count; }

private:
	Buffer m_buffer{ nullptr };
	void* m_data;
	size_t  m_size;
	int m_count{ 0 };
};


class Mesh
{
public:
	Mesh() {};
	~Mesh() {};
	void addVertexBuffer(VertexBuffer* vertexBuffer) { m_vertexBuffers.push_back(vertexBuffer); }
	void addIndexBuffer(IndexBuffer* indexBuffer) { m_indexBuffer = indexBuffer; }

	std::vector<VertexBuffer*> getVertexBuffers() { return  m_vertexBuffers; }
	IndexBuffer* getIndexBuffer() { return m_indexBuffer; };

	int getVertexCount() { return m_vertexBuffers[0]->getCount(); } //il faut que tout les vertexBuffer aillent le meme count !!

	const std::vector<VertexBufferLayout> getVertexBufferLayouts() {
		std::vector<VertexBufferLayout> vertexBufferLayouts;
		for (auto vb : m_vertexBuffers)
		{
			VertexBufferLayout vertexBufferLayout;
			// [...] Build vertex buffer layout
			vertexBufferLayout.attributeCount = (uint32_t)vb->m_vertexAttribs.size();
			vertexBufferLayout.attributes = vb->m_vertexAttribs.data();
			// == Common to attributes from the same buffer ==
			vertexBufferLayout.arrayStride = vb->m_attribsStride;
			vertexBufferLayout.stepMode = VertexStepMode::Vertex;
			vertexBufferLayouts.push_back(vertexBufferLayout);
		}
		return vertexBufferLayouts;
	}
private:

	std::vector<VertexBuffer*> m_vertexBuffers;
	IndexBuffer* m_indexBuffer{ nullptr };
};