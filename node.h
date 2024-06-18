#pragma once

#include <variant>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "context.h"

#define UNIFORMS_MAX 15

using namespace glm;

namespace Issam {
	//TODO: supprimer d'ici
	wgpu::Texture createWhiteTexture(TextureView* pTextureView) {

		wgpu::Extent3D textureSize = { 1, 1, 1 };
		// Define the texture descriptor
		wgpu::TextureDescriptor textureDesc = {};
		textureDesc.size = textureSize;
		textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
		textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
		textureDesc.dimension = wgpu::TextureDimension::_2D;
		textureDesc.mipLevelCount = 1;
		textureDesc.sampleCount = 1;

		// Create the texture
		Texture texture = Context::getInstance().getDevice().createTexture(textureDesc);

		// Define the white pixel data
		uint8_t whitePixel[4] = { 255, 255, 255, 255 };

		// Define the texture data layout
		wgpu::TextureDataLayout textureDataLayout = {};
		textureDataLayout.offset = 0;
		textureDataLayout.bytesPerRow = 4;
		textureDataLayout.rowsPerImage = 1;

		// Define the texture copy view
		wgpu::ImageCopyTexture imageCopyTexture = {};
		imageCopyTexture.texture = texture;
		imageCopyTexture.origin = { 0, 0, 0 };
		imageCopyTexture.mipLevel = 0;

		// Write the white pixel data to the texture

		Context::getInstance().getDevice().getQueue().writeTexture(imageCopyTexture, whitePixel, sizeof(whitePixel), textureDataLayout, textureSize);

		if (pTextureView) {
			TextureViewDescriptor textureViewDesc;
			textureViewDesc.aspect = TextureAspect::All;
			textureViewDesc.baseArrayLayer = 0;
			textureViewDesc.arrayLayerCount = 1;
			textureViewDesc.baseMipLevel = 0;
			textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
			textureViewDesc.dimension = TextureViewDimension::_2D;
			textureViewDesc.format = textureDesc.format;
			*pTextureView = texture.createView(textureViewDesc);
		}

		return texture;
	}

	Sampler createDefaultSampler()
	{
		SamplerDescriptor samplerDesc;
		samplerDesc.addressModeU = AddressMode::Repeat;
		samplerDesc.addressModeV = AddressMode::Repeat;
		samplerDesc.addressModeW = AddressMode::Repeat;
		samplerDesc.magFilter = FilterMode::Linear;
		samplerDesc.minFilter = FilterMode::Linear;
		samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
		samplerDesc.lodMinClamp = 0.0f;
		samplerDesc.lodMaxClamp = 8.0f;
		samplerDesc.compare = CompareFunction::Undefined;
		samplerDesc.maxAnisotropy = 1;
		return Context::getInstance().getDevice().createSampler(samplerDesc);
	}
	//-------------------------------------------------------------------

	using Value = std::variant<float, glm::vec4, glm::mat4>;
	struct Uniform {
		std::string name;
		int handle;
		Value value;
		bool isMatrix() const { return std::holds_alternative< glm::mat4>(value); }
	};

	class UniformsBuffer
	{
	public:
		UniformsBuffer()
		{
			BufferDescriptor bufferDesc;
			bufferDesc.size = sizeof(std::array<glm::vec4, UNIFORMS_MAX>);
			// Make sure to flag the buffer as BufferUsage::Uniform
			bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
			bufferDesc.mappedAtCreation = false;
			m_uniformBuffer = Context::getInstance().getDevice().createBuffer(bufferDesc);

			Context::getInstance().getDevice().getQueue().writeBuffer(m_uniformBuffer, 0, &m_uniformsData, sizeof(std::array<vec4, UNIFORMS_MAX>));
		};
		~UniformsBuffer() = default;

		uint16_t allocateVec4() {
			assert(m_uniformIndex < UNIFORMS_MAX);
			return m_uniformIndex++;
		}

		uint16_t allocateMat4()
		{
			assert(m_uniformIndex + 4 < UNIFORMS_MAX);
			uint16_t currentHandle = m_uniformIndex;
			m_uniformIndex += 4;
			return currentHandle;
		}

		void set(uint16_t handle, const Value& value)
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

		Buffer getBuffer() { return m_uniformBuffer; }

	private:
		std::array<vec4, UNIFORMS_MAX> m_uniformsData{};
		uint16_t m_uniformIndex = 0;
		Buffer m_uniformBuffer{ nullptr };
	};

	class Material {
	public:
		Material()= default;
		~Material() = default;
		void addTexture(const std::string& name, TextureView defaultTextureView) { m_textures.push_back({ name, defaultTextureView }); }

		void setTexture(const std::string& name, TextureView textureView)
		{
			auto it = std::find_if(m_textures.begin(), m_textures.end(), [name](const std::pair<std::string, TextureView>& obj) {
				return obj.first == name;
			});
			assert(it != m_textures.end());
			it->second = textureView;
			m_dirtyBindGroup = true;
		}


		void addSampler(const std::string& name, Sampler defaultSampler) { m_samplers.push_back({ name, defaultSampler }); }

		void setSampler(const std::string& name, Sampler sampler)
		{
			auto it = std::find_if(m_samplers.begin(), m_samplers.end(), [name](const std::pair<std::string, Sampler>& obj) {
				return obj.first == name;
			});
			assert(it != m_samplers.end());
			it->second = sampler;
			m_dirtyBindGroup = true;
		}


		void addUniform(std::string name, const Value& defaultValue) {
			uint16_t handle = -1;
			if (std::holds_alternative< glm::vec4>(defaultValue) || std::holds_alternative< float>(defaultValue))
				handle = m_uniformsBuffer.allocateVec4();
			else if (std::holds_alternative< glm::mat4x4>(defaultValue))
				handle = m_uniformsBuffer.allocateMat4();
			else
				assert(false);
			m_uniformsBuffer.set(handle, defaultValue);
			m_uniforms.push_back({ name, handle, defaultValue });
		};


		Uniform& getUniform(std::string name) {
			auto it = std::find_if(m_uniforms.begin(), m_uniforms.end(), [name](const Uniform& obj) {
				return obj.name == name;
			});
			assert(it != m_uniforms.end());
			return *it;
		}


		void setUniform(std::string name, const Value& value)
		{
			auto& uniform = getUniform(name);
			uniform.value = value;
			m_uniformsBuffer.set(uniform.handle, value);
			m_dirtyBindGroup = true;
		}

		BindGroup getBindGroup(BindGroupLayout bindGroupLayout) {
			if (m_dirtyBindGroup)
				refreshBinding(bindGroupLayout);
			return m_bindGroup;
		}

		std::vector<Uniform>& getUniforms() { return m_uniforms; }
		std::vector<std::pair<std::string, TextureView> >& getTextures() { return m_textures; }
		std::vector<std::pair<std::string, Sampler>>& getSamplers() { return m_samplers; }

	private:

		void refreshBinding(BindGroupLayout bindGroupLayout)
		{
			//To CALL AFTER build()
			int binding = 0;
			std::vector<BindGroupEntry> bindings;
			{
				// Create a binding
				BindGroupEntry uniformBinding{};
				// The index of the binding (the entries in bindGroupDesc can be in any order)
				uniformBinding.binding = binding++;
				// The buffer it is actually bound to
				uniformBinding.buffer = m_uniformsBuffer.getBuffer();
				// We can specify an offset within the buffer, so that a single buffer can hold
				// multiple uniform blocks.
				uniformBinding.offset = 0;
				// And we specify again the size of the buffer.
				uniformBinding.size = sizeof(std::array<vec4, UNIFORMS_MAX>);
				bindings.push_back(uniformBinding);
			}

			for (const auto& texture : m_textures)
			{
				BindGroupEntry textureBinding{};
				// The index of the binding (the entries in bindGroupDesc can be in any order)
				textureBinding.binding = binding++;
				// The buffer it is actually bound to
				textureBinding.textureView = texture.second;
				bindings.push_back(textureBinding);
			}

			for (const auto& sampler : m_samplers)
			{
				BindGroupEntry samplerBinding{};
				// The index of the binding (the entries in bindGroupDesc can be in any order)
				samplerBinding.binding = binding++;
				// The buffer it is actually bound to
				samplerBinding.sampler = sampler.second;
				bindings.push_back(samplerBinding);
			}

			// A bind group contains one or multiple bindings
			BindGroupDescriptor bindGroupDesc;
			bindGroupDesc.label = "bindGroup 0 : global uniforms";
			bindGroupDesc.layout = bindGroupLayout;
			// There must be as many bindings as declared in the layout!
			bindGroupDesc.entryCount = (uint32_t)bindings.size();
			bindGroupDesc.entries = bindings.data();
			m_bindGroup = Context::getInstance().getDevice().createBindGroup(bindGroupDesc);
			m_dirtyBindGroup = false;
		}

		bool m_dirtyBindGroup = true;
		
		BindGroup m_bindGroup{ nullptr };

		std::vector<Uniform> m_uniforms{};
		std::vector<std::pair<std::string, TextureView> > m_textures{};
		std::vector<std::pair<std::string, Sampler>> m_samplers{};
		UniformsBuffer m_uniformsBuffer{};

	};

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

			TextureView whiteTextureView = nullptr;
			Texture  whiteTexture = Issam::createWhiteTexture(&whiteTextureView);
			//TextureManager().getInstance().add("whiteTex", whiteTextureView);

			// Create a sampler
			Sampler defaultSampler = Issam::createDefaultSampler();

			material = new Material();
			material->addUniform("baseColorFactor", glm::vec4(1.0f));
			material->addUniform("time", 0.0f);

			material->addTexture("baseColorTexture", whiteTextureView);
			material->addSampler("defaultSampler", defaultSampler);
		};


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

		Node(const Node& other) {
			name = other.name;
			children = other.children;
			meshId = other.meshId;
			material = other.material;
			nodeUniformBuffer = other.nodeUniformBuffer;
		}

		void setTransform(glm::mat4 in_transform) { 
			nodeProperties.transform = in_transform;
			updateUniformBuffer();
		}
		glm::mat4& getTransform() {
			return nodeProperties.transform;
		}
		std::string name;
		std::vector<Node*> children;
		std::string meshId;

		Issam::Material* material;

	private:
		void updateUniformBuffer() { Context::getInstance().getDevice().getQueue().writeBuffer(nodeUniformBuffer, 0, &nodeProperties, sizeof(NodeProperties)); }

		NodeProperties nodeProperties;
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

	class Scene {
	public:	
		void setNodes(std::vector<Issam::Node*> nodes) {
			std::vector<Issam::Node*> flatNodes;
			for (auto rootNode : nodes) {
				flattenNodes(rootNode, glm::mat4(1.0f), flatNodes);
			}
			m_nodes = flatNodes;
		}

		void setCamera(Camera* in_camera)
		{
			m_camera = in_camera;
		}

		Issam::Camera* getCamera() { return m_camera; }
		std::vector<Issam::Node*>& getNodes() { return m_nodes; }
	private:
		void flattenNodes(Issam::Node* node, glm::mat4 parentTransform, std::vector<Issam::Node*>& flatNodes) {
			glm::mat4 globalTransform = parentTransform * node->getTransform();
			Issam::Node* flatNode = new Node(*node); // I need a copy
			flatNode->setTransform(globalTransform);
			flatNode->children.clear();
			flatNodes.push_back(flatNode);

			for (auto child : node->children) {
				flattenNodes(child, globalTransform, flatNodes);
			}
		}

		std::vector<Issam::Node*> m_nodes;
		Issam::Camera* m_camera;
	};
}