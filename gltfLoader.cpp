#include "gltfLoader.h"

#include "material.h"


#include "stb_image.h"

void GltfLoader::load(const std::string& filepath) {
	entt::entity gltfEntity = m_scene->addEntity();
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;
	std::string extension = Utils::getFileExtension(filepath);
	bool ret = false;
	if (extension == "gltf")
		ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
	else if (extension == "glb")
		ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
	else
		assert(false);

	if (!warn.empty()) {
		std::cout << "WARN: " << warn << std::endl;
	}

	if (!err.empty()) {
		std::cerr << "ERR: " << err << std::endl;
	}

	if (!ret) {
		return;
	}

	std::string baseDir = Utils::GetBaseDir(filepath);
	for (auto& gltfTexture : model.textures)
	{
		const tinygltf::Image& gltfImage = model.images[gltfTexture.source];

		unsigned char* pixelData;
		TextureView textureView = nullptr;

		if (!gltfImage.uri.empty()) {
			std::smatch matches;
			if (std::regex_search(gltfImage.uri, matches, base64Pattern)) {
				//Base64
				loadImageFromBase64(gltfImage.uri, &textureView);
			}
			else {
				//Relative path
				std::string texturePath = baseDir + "/" + gltfImage.uri;
				Utils::loadImageFromPath(texturePath, &textureView);
			}
		}
		//else if (!gltfImage.image.empty()) {
		//	//Load image from a vector of unsigned char
		//	loadImageFromVector(gltfImage.image, &textureView);
		//}
		else
		{
			//Buffer
			loadImageFromBuffer(model, gltfImage, &textureView);
		}
		std::string imageId = generateTextureId(gltfImage.uri, baseDir, gltfTexture.source);
		TextureManager().getInstance().add(imageId, textureView);
		m_sourceToId[gltfTexture.source] = imageId;
	}

	for (const auto& sceneNodeIndex : model.scenes[model.defaultScene].nodes) {
		entt::entity rootNode = m_scene->addEntity();
		m_scene->addChild(gltfEntity, rootNode);
		loadNode(model, model.nodes[sceneNodeIndex], rootNode, m_scene, sceneNodeIndex);
		//scene.push_back(rootNode);
	}

	//return scene;
	//return gltfEntity;
	m_entity = gltfEntity;
};

void GltfLoader::unload()
{
	for (auto& texId : m_sourceToId)
	{
		TextureManager().getInstance().remove(texId.second);
	}
	m_scene->removeEntity(m_entity);
}




// Base64 decoding function
std::string base64_decode(const std::string& in) {
	std::string out;
	std::vector<int> T(256, -1);
	for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
	int val = 0, valb = -8;
	for (unsigned char c : in) {
		if (T[c] == -1) break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0) {
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}



Texture GltfLoader::loadImageFromBase64(const std::string& base64, TextureView* pTextureView) {
	int width, height, channels;
	std::string base64Data = base64.substr(base64.find(",") + 1);
	std::string decodedData = base64_decode(base64Data);
	unsigned char* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(decodedData.data()), decodedData.size(), &width, &height, &channels, 4);
	if (!data) {
		std::cerr << "Failed to load image from Base64 data" << std::endl;
	}
	return Utils::loadTexture(data, width, height, channels, pTextureView);
}

Texture GltfLoader::loadImageFromVector(const std::vector<unsigned char>& imageVector, TextureView* pTextureView) {
	int width, height, channels;
	unsigned char* data = stbi_load_from_memory(imageVector.data(), imageVector.size(), &width, &height, &channels, 4);
	if (!data) {
		std::cerr << "Failed to load image from vector data" << std::endl;
	}
	return Utils::loadTexture(data, width, height, channels, pTextureView);
}

// Load image from GLTF buffer
Texture GltfLoader::loadImageFromBuffer(const tinygltf::Model& model, const tinygltf::Image& image, TextureView* pTextureView) {
	int width, height, channels;
	const tinygltf::BufferView& bufferView = model.bufferViews[image.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
	const unsigned char* dataBuffer = buffer.data.data() + bufferView.byteOffset;
	size_t size = bufferView.byteLength;
	unsigned char* data = stbi_load_from_memory(dataBuffer, size, &width, &height, &channels, 4);
	if (!data) {
		std::cerr << "Failed to load image from GLTF buffer" << std::endl;
	}

	return Utils::loadTexture(data, width, height, channels, pTextureView);
}

std::string GltfLoader::generateTextureId(const std::string& uri, const std::string& baseDir, int source)
{
	if (!uri.empty()) {
		std::smatch matches;
		if (std::regex_search(uri, matches, base64Pattern)) {
			//Base64
			return "image64_" + std::to_string(source);
		}
		else {
			//Relative path
			std::string texturePath = baseDir + "/" + uri;
			return uri;
		}
	}
	else
	{
		//Buffer
		return "imageBuffer_" + std::to_string(source);

	}
}

void GltfLoader::loadMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, entt::entity parent, Issam::Scene* scene)
{
	int primitiveIdx = 0;
	for (const auto& primitive : mesh.primitives)
	{
		entt::entity entity = scene->addEntity();
		scene->addChild(parent, entity);
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;
		// Process vertex attributes
		const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
		if (posAccessor.bufferView == -1) return;
		const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
		const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

		const tinygltf::Accessor* normAccessor = nullptr;
		const tinygltf::BufferView* normBufferView = nullptr;
		const tinygltf::Buffer* normBuffer = nullptr;

		const tinygltf::Accessor* uvAccessor = nullptr;
		const tinygltf::BufferView* uvBufferView = nullptr;
		const tinygltf::Buffer* uvBuffer = nullptr;

		const tinygltf::Accessor* tangentAccessor = nullptr;
		const tinygltf::BufferView* tangentBufferView = nullptr;
		const tinygltf::Buffer* tangentBuffer = nullptr;

		if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
			normAccessor = &model.accessors[primitive.attributes.find("NORMAL")->second];
			normBufferView = &model.bufferViews[normAccessor->bufferView];
			normBuffer = &model.buffers[normBufferView->buffer];
		}

		if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
			uvAccessor = &model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
			uvBufferView = &model.bufferViews[uvAccessor->bufferView];
			uvBuffer = &model.buffers[uvBufferView->buffer];
		}

		if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
			tangentAccessor = &model.accessors[primitive.attributes.find("TANGENT")->second];
			tangentBufferView = &model.bufferViews[tangentAccessor->bufferView];
			tangentBuffer = &model.buffers[tangentBufferView->buffer];
		}

		for (size_t i = 0; i < posAccessor.count; ++i) {
			Vertex vertex = {};

			size_t posIndex = i * posAccessor.ByteStride(posBufferView) + posAccessor.byteOffset + posBufferView.byteOffset;
			vertex.position = glm::vec3(
				*reinterpret_cast<const float*>(&posBuffer.data[posIndex + 0 * sizeof(float)]),
				*reinterpret_cast<const float*>(&posBuffer.data[posIndex + 1 * sizeof(float)]),
				*reinterpret_cast<const float*>(&posBuffer.data[posIndex + 2 * sizeof(float)]));

			if (normAccessor) {
				size_t normIndex = i * normAccessor->ByteStride(*normBufferView) + normAccessor->byteOffset + normBufferView->byteOffset;
				vertex.normal = glm::vec3(
					*reinterpret_cast<const float*>(&normBuffer->data[normIndex + 0 * sizeof(float)]),
					*reinterpret_cast<const float*>(&normBuffer->data[normIndex + 1 * sizeof(float)]),
					*reinterpret_cast<const float*>(&normBuffer->data[normIndex + 2 * sizeof(float)]));
			}

			if (uvAccessor) {
				size_t uvIndex = i * uvAccessor->ByteStride(*uvBufferView) + uvAccessor->byteOffset + uvBufferView->byteOffset;
				vertex.uv = glm::vec2(
					*reinterpret_cast<const float*>(&uvBuffer->data[uvIndex + 0 * sizeof(float)]),
					*reinterpret_cast<const float*>(&uvBuffer->data[uvIndex + 1 * sizeof(float)]));
			}

			if (tangentAccessor) {
				size_t tangentIndex = i * tangentAccessor->ByteStride(*tangentBufferView) + tangentAccessor->byteOffset + tangentBufferView->byteOffset;
				vertex.tangent = glm::vec3(
					*reinterpret_cast<const float*>(&tangentBuffer->data[tangentIndex + 0 * sizeof(float)]),
					*reinterpret_cast<const float*>(&tangentBuffer->data[tangentIndex + 1 * sizeof(float)]),
					*reinterpret_cast<const float*>(&tangentBuffer->data[tangentIndex + 2 * sizeof(float)]));
			}

			vertices.push_back(vertex);
		}
		MeshPtr myMesh = std::make_shared<Mesh>();
		myMesh->setVertices(vertices);

		if (primitive.indices != -1)
		{
			// Process indices
			const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
			const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
			const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

			for (size_t i = 0; i < indexAccessor.count; ++i) {
				size_t index = i * indexAccessor.ByteStride(indexBufferView) + indexAccessor.byteOffset + indexBufferView.byteOffset;
				uint32_t indexValue;
				switch (indexAccessor.componentType) {
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					indexValue = *reinterpret_cast<const uint8_t*>(&indexBuffer.data[index]);
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					indexValue = *reinterpret_cast<const uint16_t*>(&indexBuffer.data[index]);
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					assert(false); //NOT SUPPORTED
					indexValue = *reinterpret_cast<const uint32_t*>(&indexBuffer.data[index]);
					break;
				default:
					throw std::runtime_error("Unsupported index component type");
				}
				indices.push_back(indexValue);
			}
			myMesh->setIndices(indices);
		}


		Issam::MeshRenderer meshRenderer;
		meshRenderer.mesh = myMesh;
		if (primitive.material != -1)
		{
			Material* material = new Material();
			auto& gltfMaterial = model.materials[primitive.material];
			auto& baseColorFactor = gltfMaterial.pbrMetallicRoughness.baseColorFactor;
			material->setAttribute("baseColorFactor", glm::make_vec4(baseColorFactor.data()));

			int baseColorTextureIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
			if (baseColorTextureIndex >= 0 && baseColorTextureIndex < model.textures.size())
			{
				const tinygltf::Texture& gltfTexture = model.textures[baseColorTextureIndex];
				//	const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
				std::string id = m_sourceToId.find(gltfTexture.source)->second;
				material->setAttribute("baseColorTexture", TextureManager::getInstance().getTextureView(id));
			}

			int metallicRoughnessIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
			if (metallicRoughnessIndex >= 0 && metallicRoughnessIndex < model.textures.size())
			{
				const tinygltf::Texture& gltfTexture = model.textures[metallicRoughnessIndex];
				//const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
				std::string id = m_sourceToId.find(gltfTexture.source)->second;
				material->setAttribute("metallicRoughnessTexture", TextureManager::getInstance().getTextureView(id));
			}

			auto& metallicFactor = gltfMaterial.pbrMetallicRoughness.metallicFactor;
			material->setAttribute("metallicFactor", (float)metallicFactor);

			auto& roughnessFactor = gltfMaterial.pbrMetallicRoughness.roughnessFactor;
			material->setAttribute("roughnessFactor", (float)roughnessFactor);

			material->setAttribute("unlitMaterialModel", "colorFactor", glm::vec4(1.0, 0.5, 0.0, 1.0), 0);
			material->setAttribute("unlitMaterialModel", "colorFactor", glm::vec4(0.0), 1);

			meshRenderer.material = material;

			Issam::Filters filters;
			filters.add("pbr");
			scene->addComponent<Issam::Filters>(entity, filters);

		}

		scene->addComponent<Issam::MeshRenderer>(entity, meshRenderer);
	}
}

void GltfLoader::loadNode(const tinygltf::Model& model, const tinygltf::Node& gltfNode, entt::entity entity, Issam::Scene* scene, int idx) {
	static int nodeId = 0;
	std::string nodeName = gltfNode.name;
	if (nodeName.empty())
		nodeName = "node_" + std::to_string(idx);


	scene->addComponent<Issam::Name>(entity, Issam::Name({ nodeName }));

	const glm::vec3 T =
		gltfNode.translation.empty()
		? glm::vec3(0.0)
		: glm::make_vec3(gltfNode.translation.data());

	const glm::quat R =
		gltfNode.rotation.empty()
		? glm::quat()
		: glm::make_quat(gltfNode.rotation.data());

	const glm::vec3 S =
		gltfNode.scale.empty()
		? glm::vec3(1.0)
		: glm::make_vec3(gltfNode.scale.data());

	//node->setTransform(glm::translate(glm::mat4(1.0), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0), S));
	scene->setLocalTransform(entity, glm::translate(glm::mat4(1.0), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0), S));

	if (!gltfNode.matrix.empty()) {
		scene->setLocalTransform(entity, glm::make_mat4(gltfNode.matrix.data()));
		//node->setTransform(glm::make_mat4(gltfNode.matrix.data()));
	}

	if (gltfNode.mesh >= 0) {
		loadMesh(model, model.meshes[gltfNode.mesh], entity, scene);
	}

	for (int childIndex : gltfNode.children) {
		entt::entity childEntity = scene->addEntity();
		loadNode(model, model.nodes[childIndex], childEntity, scene, childIndex);
		scene->addChild(entity, childEntity);
		//node->children.push_back(childNode);
	}
}