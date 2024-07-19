
#include "utils.h"




entt::entity Utils::createBoundingBox(Issam::Scene* scene, const vec3& point1, const vec3& point2) {
	std::vector<Vertex> vertices(8);
	//std::vector<uint16_t> indices = {
	//	0, 1, 2, 2, 3, 0, // Front face
	//	4, 5, 6, 6, 7, 4, // Back face
	//	0, 1, 5, 5, 4, 0, // Bottom face
	//	2, 3, 7, 7, 6, 2, // Top face
	//	0, 3, 7, 7, 4, 0, // Left face
	//	1, 2, 6, 6, 5, 1  // Right face
	//};
	std::vector<uint16_t> indices = {
	   0, 1, 1, 2, 2, 3, 3, 0, // Bottom face
	   4, 5, 5, 6, 6, 7, 7, 4, // Top face
	   0, 4, 1, 5, 2, 6, 3, 7  // Vertical edges
	};


	vec3 minPoint = glm::min(point1, point2);
	vec3 maxPoint = glm::max(point1, point2);

	vertices[0].position = vec3(minPoint.x, minPoint.y, minPoint.z);
	vertices[1].position = vec3(maxPoint.x, minPoint.y, minPoint.z);
	vertices[2].position = vec3(maxPoint.x, maxPoint.y, minPoint.z);
	vertices[3].position = vec3(minPoint.x, maxPoint.y, minPoint.z);

	vertices[4].position = vec3(minPoint.x, minPoint.y, maxPoint.z);
	vertices[5].position = vec3(maxPoint.x, minPoint.y, maxPoint.z);
	vertices[6].position = vec3(maxPoint.x, maxPoint.y, maxPoint.z);
	vertices[7].position = vec3(minPoint.x, maxPoint.y, maxPoint.z);

	MeshPtr cubeMesh = std::make_shared<Mesh>();

	cubeMesh->setVertices(vertices);
	cubeMesh->setIndices(indices);

	entt::entity cubeEntity = scene->addEntity();
	Issam::MeshRenderer meshRenderer;
	meshRenderer.mesh = cubeMesh;
	Material* material = new Material();
	material->setAttribute("unlitMaterialModel", "colorFactor", glm::vec4(1.0, 0.0, 1.0, 1.0));
	meshRenderer.material = material;
	scene->addComponent<Issam::MeshRenderer>(cubeEntity, meshRenderer);

	Issam::Filters filters;
	filters.add("debug");
	scene->addComponent<Issam::Filters>(cubeEntity, filters);

	return cubeEntity;
}

entt::entity Utils::addLine(Issam::Scene* scene, glm::vec3 start, glm::vec3 end, glm::vec4 color) {
	std::vector<Vertex> vertices;
	vertices.push_back({ start });
	vertices.push_back({ end });
	std::vector<uint16_t> indices;
	indices.push_back(0);
	indices.push_back(1);

	MeshPtr lineMesh = std::make_shared<Mesh>();
	lineMesh->setVertices(vertices);
	lineMesh->setIndices(indices);

	Issam::MeshRenderer meshRenderer;
	meshRenderer.mesh = lineMesh;
	Material* material = new Material();
	material->setAttribute("unlitMaterialModel", "colorFactor", color);
	meshRenderer.material = material;

	entt::entity lineEntity = scene->addEntity();
	scene->addComponent<Issam::MeshRenderer>(lineEntity, meshRenderer);

	Issam::Filters filters;
	filters.add("debug");
	scene->addComponent<Issam::Filters>(lineEntity, filters);
	return lineEntity;
}

entt::entity Utils::addAxes(Issam::Scene* scene)
{
	glm::vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
	glm::vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);

	// Add X axis (red)
	entt::entity x_axis = addLine(scene, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), red);

	// Add Y axis (green)
	entt::entity y_axis = addLine(scene, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), green);

	// Add Z axis (blue)
	entt::entity z_axis = addLine(scene, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), blue);

	entt::entity axes = scene->addEntity();
	scene->addChild(axes, x_axis);
	scene->addChild(axes, y_axis);
	scene->addChild(axes, z_axis);
	return axes;
}

wgpu::Texture Utils::CreateWhiteTexture(TextureView* pTextureView) {

	wgpu::Extent3D textureSize = { 1, 1, 1 };
	// Define the texture descriptor
	wgpu::TextureDescriptor textureDesc = {};
	textureDesc.size = textureSize;
	textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
	textureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
	textureDesc.dimension = wgpu::TextureDimension::e2D;
	textureDesc.mipLevelCount = 1;
	textureDesc.sampleCount = 1;

	// Create the texture
	Texture texture = Context::getInstance().getDevice().CreateTexture(&textureDesc);

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

	Context::getInstance().getDevice().GetQueue().WriteTexture(&imageCopyTexture, whitePixel, sizeof(whitePixel), &textureDataLayout, &textureSize);

	if (pTextureView) {
		TextureViewDescriptor textureViewDesc;
		textureViewDesc.aspect = TextureAspect::All;
		textureViewDesc.baseArrayLayer = 0;
		textureViewDesc.arrayLayerCount = 1;
		textureViewDesc.baseMipLevel = 0;
		textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
		textureViewDesc.dimension = TextureViewDimension::e2D;
		textureViewDesc.format = textureDesc.format;
		*pTextureView = texture.CreateView(&textureViewDesc);
	}

	return texture;
}

std::string Utils::loadFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		std::cerr << path.c_str() << " not found ! " << std::endl;
	}
	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string str = std::string(size, ' ');
	file.seekg(0);
	file.read(str.data(), size);
	return str;
};

Sampler Utils::createDefaultSampler()
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
	return Context::getInstance().getDevice().CreateSampler(&samplerDesc);
}

std::string Utils::GetBaseDir(const std::string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\") + 1);
	return "";
}

std::string Utils::getFileExtension(const std::string& filePath) {
	size_t dotPosition = filePath.find_last_of('.');
	if (dotPosition == std::string::npos) {
		// No extension found
		return "";
	}

	size_t slashPosition = filePath.find_last_of("/\\");
	if (slashPosition != std::string::npos && slashPosition > dotPosition) {
		// Dot found, but it's part of the directory name
		return "";
	}

	return filePath.substr(dotPosition + 1);
}

// Auxiliary function for loadTexture
template<typename T>
static void writeMipMaps(
	Texture texture,
	Extent3D textureSize,
	uint32_t mipLevelCount,
	const T* pixelData
) {
	Queue queue = Context::getInstance().getDevice().GetQueue();

	// Arguments telling which part of the texture we upload to
	ImageCopyTexture destination;
	destination.texture = texture;
	destination.origin = { 0, 0, 0 };
	destination.aspect = TextureAspect::All;

	// Arguments telling how the C++ side pixel memory is laid out
	TextureDataLayout source;
	source.offset = 0;

	// Create image data
	Extent3D mipLevelSize = textureSize;
	std::vector<T> previousLevelPixels;
	Extent3D previousMipLevelSize;
	for (uint32_t level = 0; level < mipLevelCount; ++level) {
		std::vector<T> pixels(4 * mipLevelSize.width * mipLevelSize.height);
		if (level == 0) {
			// We cannot really avoid this copy since we need this
			// in previousLevelPixels at the next iteration
			memcpy(pixels.data(), pixelData, pixels.size() * sizeof(T));
		}
		else {
			// Create mip level data
			for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
				for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
					T* p = &pixels[4 * (j * mipLevelSize.width + i)];
					// Get the corresponding 4 pixels from the previous level
					T* p00 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 0))];
					T* p01 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 1))];
					T* p10 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 0))];
					T* p11 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 1))];
					// Average
					p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / (T)4;
					p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / (T)4;
					p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / (T)4;
					p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / (T)4;
				}
			}
		}

		// Upload data to the GPU texture
		destination.mipLevel = level;
		source.bytesPerRow = 4 * mipLevelSize.width * sizeof(T);
		source.rowsPerImage = mipLevelSize.height;
		queue.WriteTexture(&destination, pixels.data(), pixels.size() * sizeof(T), &source, &mipLevelSize);

		previousLevelPixels = std::move(pixels);
		previousMipLevelSize = mipLevelSize;
		mipLevelSize.width /= 2;
		mipLevelSize.height /= 2;
	}
}

static uint32_t bit_width(uint32_t m) {
	if (m == 0) return 0;
	else { uint32_t w = 0; while (m >>= 1) ++w; return w; }
}
Texture Utils::loadTexture(void* pixelData, int& width, int& height, int& channels, TextureFormat format, TextureView* pTextureView) {

	assert(pixelData);

	// Use the width, height, channels and data variables here
	TextureDescriptor textureDesc;
	textureDesc.dimension = TextureDimension::e2D;
	textureDesc.format = format; // by convention for bmp, png and jpg file. Be careful with other formats.
	textureDesc.size = { (unsigned int)width, (unsigned int)height, 1 };
	textureDesc.mipLevelCount = bit_width(std::max(textureDesc.size.width, textureDesc.size.height));
	textureDesc.sampleCount = 1;
	textureDesc.usage = TextureUsage::TextureBinding | TextureUsage::CopyDst;
	textureDesc.viewFormatCount = 0;
	textureDesc.viewFormats = nullptr;
	Texture texture = Context::getInstance().getDevice().CreateTexture(&textureDesc);

	// Upload data to the GPU texture
	if(format != TextureFormat::RGBA32Float)
		writeMipMaps<unsigned char>(texture, textureDesc.size, textureDesc.mipLevelCount, (unsigned char*)pixelData);
	else
		writeMipMaps<float>(texture, textureDesc.size, textureDesc.mipLevelCount, (float*)pixelData);

	stbi_image_free(pixelData);
	// (Do not use data after this)

	if (pTextureView) {
		TextureViewDescriptor textureViewDesc;
		textureViewDesc.aspect = TextureAspect::All;
		textureViewDesc.baseArrayLayer = 0;
		textureViewDesc.arrayLayerCount = 1;
		textureViewDesc.baseMipLevel = 0;
		textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
		textureViewDesc.dimension = TextureViewDimension::e2D;
		textureViewDesc.format = textureDesc.format;
		*pTextureView = texture.CreateView(&textureViewDesc);
	}

	return texture;
}

Texture Utils::loadImageFromPath(const std::string& path, TextureView* pTextureView, bool hdr) {
	int width, height, channels;
	void* data = nullptr;
	TextureFormat format = TextureFormat::Undefined;
	if (!hdr)
	{
		data = stbi_load(path.c_str(), &width, &height, &channels, 4);
		format = TextureFormat::RGBA8Unorm;
	}
	else
	{
		data = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
		format = TextureFormat::RGBA32Float;
	}

	if (!data) {
		std::cerr << "Failed to load image from path: " << path << std::endl;
	}

	return loadTexture(data, width, height, channels, format, pTextureView);
}