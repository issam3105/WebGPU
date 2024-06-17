#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"

namespace Issam {
	struct NodeProperties {
		glm::mat4 transform{ glm::mat4(1) };
	};
	class Node {
	public:
		Node() {
			BufferDescriptor bufferDesc;
			bufferDesc.label = name.c_str();
			bufferDesc.mappedAtCreation = false;
			bufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
			bufferDesc.size = sizeof(NodeProperties);
			nodeUniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);
			Context::getInstance().getDevice().getQueue().writeBuffer(nodeUniformBuffer, 0, &nodeProperties, sizeof(NodeProperties));

		};

		void updateUniformBuffer() { Context::getInstance().getDevice().getQueue().writeBuffer(nodeUniformBuffer, 0, &nodeProperties, sizeof(NodeProperties)); }

		BindGroup getBindGroup(BindGroupLayout bindGroupLayout) {
			if (!dirtyBindGroup)
				return bindGroup;
			// Bind Group
			std::vector<BindGroupEntry> bindGroupEntries(1, Default);
			bindGroupEntries[0].binding = 0;
			bindGroupEntries[0].buffer = nodeUniformBuffer;
			bindGroupEntries[0].size = sizeof(NodeProperties);

			BindGroupDescriptor bindGroupDesc;
			bindGroupDesc.label = name.c_str();
			bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
			bindGroupDesc.entries = bindGroupEntries.data();
			bindGroupDesc.layout = bindGroupLayout;
			bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
			dirtyBindGroup = false;
			return bindGroup;
		}

		std::string name;
		NodeProperties nodeProperties;
		std::vector<Node> children;
		std::string meshId;
		Buffer nodeUniformBuffer{ nullptr };
		BindGroup bindGroup{ nullptr };
		bool dirtyBindGroup = true;

	};

	struct CameraProperties
	{
		glm::mat4 view{ glm::mat4(1.0) };
		glm::mat4 projection{ glm::mat4(1.0) };
	};

	class Camera {
	public:
		Camera() {
			BufferDescriptor bufferDesc;
			bufferDesc.label = "camera";
			bufferDesc.mappedAtCreation = false;
			bufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
			bufferDesc.size = sizeof(CameraProperties);
			cameraUniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);
			Context::getInstance().getDevice().getQueue().writeBuffer(cameraUniformBuffer, 0, &cameraProperties, sizeof(CameraProperties));
		}
		void setView(glm::mat4 in_view) { 
			cameraProperties.view = in_view; 
		    updateUniformBuffer();
		}
		void setProjection(glm::mat4 in_projection) { 
			cameraProperties.projection = in_projection;
			updateUniformBuffer();
		}	

		BindGroup getBindGroup(BindGroupLayout bindGroupLayout) {
			if (!dirtyBindGroup)
				return bindGroup;
			// Bind Group
			std::vector<BindGroupEntry> bindGroupEntries(1, Default);
			bindGroupEntries[0].binding = 0;
			bindGroupEntries[0].buffer = cameraUniformBuffer;
			bindGroupEntries[0].size = sizeof(CameraProperties);

			BindGroupDescriptor bindGroupDesc;
			bindGroupDesc.label = "camera";
			bindGroupDesc.entryCount = static_cast<uint32_t>(bindGroupEntries.size());
			bindGroupDesc.entries = bindGroupEntries.data();
			bindGroupDesc.layout = bindGroupLayout;
			bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
			dirtyBindGroup = false;
			return bindGroup;
		}
	private:
		void updateUniformBuffer() { Context::getInstance().getDevice().getQueue().writeBuffer(cameraUniformBuffer, 0, &cameraProperties, sizeof(CameraProperties)); }
		CameraProperties cameraProperties;
		Buffer cameraUniformBuffer{ nullptr };
		BindGroup bindGroup{ nullptr };
		bool dirtyBindGroup = true;
	};
}