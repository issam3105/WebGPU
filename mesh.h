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

	//void addVertexAttrib(const std::string& attribName, int location, VertexFormat format, int offset)
	//{
	//	VertexAttribute vertexAttribute;
	//	vertexAttribute.shaderLocation = location;
	//	vertexAttribute.format = format;
	//	vertexAttribute.offset = offset;
	//	m_vertexAttribs.push_back(vertexAttribute);
	//}
	//void setAttribsStride(uint64_t attribsStride) { m_attribsStride = attribsStride; }


	const Buffer& getBuffer()const { return m_buffer; }
	const uint64_t getSize() const { return m_size; }
	const int getCount() const { return m_count; }

private:
	Buffer m_buffer{ nullptr };
	void* m_data;
	size_t  m_size;
	int m_count{ 0 };
	//std::vector<VertexAttribute> m_vertexAttribs;
	//uint64_t m_attribsStride = 0;

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

struct Vertex {
	vec3 position = vec3(0.0, 0.0, 0.0);
	vec3 normal = vec3(0.0, 0.0, 0.0);;
	vec3 tangent = vec3(0.0, 0.0, 0.0);;
	vec2 uv = vec2(0.0, 0.0);;
};


class Mesh
{
public:
	Mesh() {};
	~Mesh() {};
	void setVertices(std::vector<Vertex> vertices) { 
		m_vertices = vertices; 
		int indexCount = static_cast<int>(vertices.size());
		int vertexCount = static_cast<int>(vertices.size());
		auto* data = vertices.data();
		m_vertexBuffer = new VertexBuffer(vertices.data(), vertices.size() * sizeof(Vertex), vertexCount);
	}
	void setIndices(std::vector<uint16_t> indices) { m_indices = indices; }

	VertexBuffer* getVertexBuffer() { return  m_vertexBuffer; }
	IndexBuffer* getIndexBuffer() { return m_indexBuffer; };

	int getVertexCount() { return m_vertices.size(); } 

private:

	VertexBuffer* m_vertexBuffer;
	IndexBuffer* m_indexBuffer{ nullptr };

	std::vector<uint16_t> m_indices;
	std::vector<Vertex> m_vertices;
};