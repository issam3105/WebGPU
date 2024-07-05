#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define TINYOBJLOADER_IMPLEMENTATION // add this to exactly 1 of your C++ files
#include "tiny_obj_loader.h"

#include <entt/entt.hpp>

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <array>
#include <variant>
#include "webgpu-utils.h"

#include "context.h"
#include "shader.h"
#include "pipeline.h"
#include "pass.h"
#include "mesh.h"
#include "managers.h"
#include "imgui_wrapper.h"
#include "utils.h"
#include "renderer.h"
#include "material.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace wgpu;
namespace fs = std::filesystem;

using namespace glm;

constexpr float PI = 3.14159265358979323846f;

#ifdef WEBGPU_BACKEND_WGPU
TextureFormat swapChainFormat = surface.getPreferredFormat(adapter);
#else
TextureFormat swapChainFormat = TextureFormat::BGRA8Unorm;
#endif

TextureFormat depthTextureFormat = TextureFormat::Depth24Plus;

int m_winWidth = 1280;
int m_winHeight = 720;

Issam::Scene* scene{nullptr};

struct CameraState {
	// angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
	// angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
	vec2 angles = { 0.8f, 0.5f };
	// zoom is the position of the camera along its local forward axis, affected by the scroll wheel
	float zoom = -1.2f;
};

struct DragState {
	// Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
	bool active = false;
	// The position of the mouse at the beginning of the drag action
	vec2 startMouse;
	// The camera state at the beginning of the drag action
	CameraState startCameraState;

	// Constant settings
	float sensitivity = 0.01f;
	float scrollSensitivity = 0.1f;

	// Inertia
	vec2 velocity = { 0.0, 0.0 };
	vec2 previousDelta;
	float intertia = 0.9f;
};


DragState m_drag;
CameraState m_cameraState;

entt::entity cameraEntity;

void updateViewMatrix(Issam::Scene* scene) {
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	mat4x4 viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1));

	auto& camera = scene->getRegistry().get<Issam::Camera>(cameraEntity);
	camera.m_pos = position;
	camera.m_view = viewMatrix;
	scene->getRegistry().patch<Issam::Camera>(cameraEntity);
}

std::vector<std::string> GetFiles(const std::string& directoryPath, std::string extension) {
	std::vector<std::string> gltfFiles;

	for (const auto& entry : fs::recursive_directory_iterator(directoryPath)) {
		if (entry.is_regular_file() && entry.path().extension() == extension) {
			gltfFiles.push_back(entry.path().string());
		}
	}

	return gltfFiles;
}

glm::mat4 computeRotationMatrix(const glm::vec3& rotation) {
	glm::mat4 rx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 ry = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 rz = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
	return rz * ry * rx;
}

TextureView createBuffer(uint32_t width, uint32_t height, WGPUTextureFormat format)
{
	// Create the depth texture
	TextureDescriptor textureDesc;
	textureDesc.dimension = TextureDimension::_2D;
	textureDesc.format = format;
	textureDesc.mipLevelCount = 1;
	textureDesc.sampleCount = 1;
	textureDesc.size = { width, height, 1 };
	textureDesc.usage = TextureUsage::RenderAttachment | TextureUsage::TextureBinding;
	textureDesc.viewFormatCount = 1;
	textureDesc.viewFormats = (WGPUTextureFormat*)&format;
	Texture depthTexture = Context::getInstance().getDevice().createTexture(textureDesc);

	// Create the view of the depth texture manipulated by the rasterizer
	TextureViewDescriptor textureViewDesc;
	textureViewDesc.aspect = TextureAspect::All;;//TODO TextureAspect::DepthOnly; for depth
	textureViewDesc.baseArrayLayer = 0;
	textureViewDesc.arrayLayerCount = 1;
	textureViewDesc.baseMipLevel = 0;
	textureViewDesc.mipLevelCount = 1;
	textureViewDesc.dimension = TextureViewDimension::_2D;
	textureViewDesc.format = format;
	return depthTexture.createView(textureViewDesc);
}

glm::vec3 getRayFromMouse(float mouseX, float mouseY, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, int screenWidth, int screenHeight) {
	float x = (2.0f * mouseX) / screenWidth - 1.0f;
	float y = 1.0f - (2.0f * mouseY) / screenHeight;
	float z = 1.0f;
	glm::vec3 rayNDS(x, y, z);

	glm::vec4 rayClip(rayNDS.x, rayNDS.y, -1.0f, 1.0f);

	glm::vec4 rayEye = glm::inverse(projectionMatrix) * rayClip;
	rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

	glm::vec3 rayWorld = glm::inverse(viewMatrix) * rayEye;
	rayWorld = glm::normalize(rayWorld);

	return rayWorld;
}

// Fonction pour tester l'intersection entre un rayon et une bounding box
using BoundingBox = std::pair<glm::vec3, glm::vec3>;
bool rayIntersectsBoundingBox(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const BoundingBox& box, float& t) {
	float tmin = (box.first.x - rayOrigin.x) / rayDirection.x;
	float tmax = (box.second.x - rayOrigin.x) / rayDirection.x;

	if (tmin > tmax) std::swap(tmin, tmax);

	float tymin = (box.first.y - rayOrigin.y) / rayDirection.y;
	float tymax = (box.second.y - rayOrigin.y) / rayDirection.y;

	if (tymin > tmax) std::swap(tymin, tymax);

	if ((tmin > tymax) || (tymin > tmax)) return false;

	if (tymin > tmin) tmin = tymin;
	if (tymax < tmax) tmax = tymax;

	float tzmin = (box.first.z - rayOrigin.z) / rayDirection.z;
	float tzmax = (box.second.z - rayOrigin.z) / rayDirection.z;

	if (tzmin > tzmax) std::swap(tzmin, tzmax);

	if ((tmin > tzmax) || (tzmin > tmax)) return false;

	if (tzmin > tmin) tmin = tzmin;
	if (tzmax < tmax) tmax = tzmax;

	t = tmin;

	return true;
}

entt::entity pickedEntity = entt::null;

int main(int, char**) {
	if (!glfwInit()) {
		std::cerr << "Could not initialize GLFW!" << std::endl;
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(m_winWidth, m_winHeight, "Learn WebGPU", NULL, NULL);
	if (!window) {
		std::cerr << "Could not open window!" << std::endl;
		return 1;
	}

	Context::getInstance().initGraphics(window, m_winWidth, m_winHeight, swapChainFormat);

	std::vector<std::string> jpgFiles = GetFiles(DATA_DIR, ".jpg");
	std::vector<std::string> pngFiles = GetFiles(DATA_DIR, ".png");
	jpgFiles.insert(jpgFiles.end(), pngFiles.begin(), pngFiles.end());


	std::unordered_map<std::string, std::shared_ptr<wgpu::TextureView>> textures{};
	for (auto& file : jpgFiles)
	{
		TextureView textureView =nullptr;
		Texture gltfTexture = Utils::loadTexture(file, &textureView);
		TextureManager().getInstance().add(file, textureView);
	}

	TextureView whiteTextureView = nullptr;
	Texture  whiteTexture = Utils::CreateWhiteTexture(&whiteTextureView);
	TextureManager().getInstance().add("whiteTex", whiteTextureView);

	// Create a sampler
	Sampler defaultSampler = Utils::createDefaultSampler();
	SamplerManager().getInstance().add("defaultSampler", defaultSampler);
	

	Shader* backgroundShader = new Shader();
	backgroundShader->setUserCode(Utils::loadFile(DATA_DIR  "/background.wgsl"));
	backgroundShader->addVertexInput("position", 0, VertexFormat::Float32x3);
	backgroundShader->addVertexInput("normal", 1, VertexFormat::Float32x3);
	backgroundShader->addVertexInput("tangent", 2, VertexFormat::Float32x3);
	backgroundShader->addVertexInput("uv", 3, VertexFormat::Float32x2);

	backgroundShader->addVertexOutput("tangent", 0, VertexFormat::Float32x3);
	backgroundShader->addVertexOutput("normal", 1, VertexFormat::Float32x3);
	backgroundShader->addVertexOutput("uv", 2, VertexFormat::Float32x2);

	Issam::Attributed backgroundShaderAttributes;
	backgroundShaderAttributes.addAttribute("backgroundFactor", glm::vec4(1.0f));
	backgroundShaderAttributes.addAttribute("backgroundTexture", whiteTextureView);
	backgroundShaderAttributes.addAttribute("defaultSampler", defaultSampler);
	Issam::AttributedManager::getInstance().add(Issam::c_backgroundSceneAttributes, backgroundShaderAttributes);
	backgroundShader->addAttributes(Issam::c_backgroundSceneAttributes, Shader::Binding::Scene); 

	Shader* diltationShader = new Shader();
	diltationShader->setUserCode(Utils::loadFile(DATA_DIR  "/dilatation.wgsl"));
	diltationShader->addVertexInput("position", 0, VertexFormat::Float32x3);
	diltationShader->addVertexInput("normal", 1, VertexFormat::Float32x3);
	diltationShader->addVertexInput("tangent", 2, VertexFormat::Float32x3);
	diltationShader->addVertexInput("uv", 3, VertexFormat::Float32x2);

	diltationShader->addVertexOutput("tangent", 0, VertexFormat::Float32x3);
	diltationShader->addVertexOutput("normal", 1, VertexFormat::Float32x3);
	diltationShader->addVertexOutput("uv", 2, VertexFormat::Float32x2);

	Issam::Attributed diltationShaderAttributes;
	diltationShaderAttributes.addAttribute("viewTexel", glm::vec4(1.0f / m_winWidth, 1.0f/ m_winHeight, 0.0, 0.0));
	diltationShaderAttributes.addAttribute("source", whiteTextureView);
	diltationShaderAttributes.addAttribute("defaultSampler", defaultSampler);
	Issam::AttributedManager::getInstance().add(Issam::c_diltationSceneAttributes, diltationShaderAttributes);
	diltationShader->addAttributes(Issam::c_diltationSceneAttributes, Shader::Binding::Scene);

	Shader* pbrShader = new Shader();
	pbrShader->setUserCode(Utils::loadFile(DATA_DIR  "/pbr.wgsl"));
	pbrShader->addVertexInput("position", 0, VertexFormat::Float32x3);
	pbrShader->addVertexInput("normal", 1, VertexFormat::Float32x3);
	pbrShader->addVertexInput("color", 2, VertexFormat::Float32x3);
	pbrShader->addVertexInput("uv", 3, VertexFormat::Float32x2);

	pbrShader->addVertexOutput("color", 0, VertexFormat::Float32x3);
	pbrShader->addVertexOutput("normal", 1, VertexFormat::Float32x3);
	pbrShader->addVertexOutput("uv", 2, VertexFormat::Float32x2);
	pbrShader->addVertexOutput("worldPosition", 3, VertexFormat::Float32x4);

	Issam::Attributed pbrMaterialAttributes;
	pbrMaterialAttributes.addAttribute("baseColorFactor", glm::vec4(1.0f));
	pbrMaterialAttributes.addAttribute("metallicFactor", 0.5f);
	pbrMaterialAttributes.addAttribute("roughnessFactor", 0.5f);

	pbrMaterialAttributes.addAttribute("baseColorTexture", whiteTextureView);
	pbrMaterialAttributes.addAttribute("metallicRoughnessTexture", whiteTextureView);
	pbrMaterialAttributes.addAttribute("defaultSampler", defaultSampler);
	Issam::AttributedManager::getInstance().add(c_pbrMaterialAttributes, pbrMaterialAttributes);

	Issam::Attributed pbrSceneAttributes;
	pbrSceneAttributes.addAttribute("view", mat4(1.0));
	pbrSceneAttributes.addAttribute("projection", mat4(1.0));
	pbrSceneAttributes.addAttribute("cameraPosition", vec4(0.0));
	pbrSceneAttributes.addAttribute("lightDirection", vec4(1.0));

	Issam::AttributedManager::getInstance().add(Issam::c_pbrSceneAttributes, pbrSceneAttributes);

	Issam::Attributed pbrNodeAttributes;
	pbrNodeAttributes.addAttribute("model", mat4(1.0));
	Issam::AttributedManager::getInstance().add(Issam::c_pbrNodeAttributes, pbrNodeAttributes);

	pbrShader->addAttributes(c_pbrMaterialAttributes, Shader::Binding::Material);
	pbrShader->addAttributes(Issam::c_pbrSceneAttributes, Shader::Binding::Scene);
	pbrShader->addAttributes(Issam::c_pbrNodeAttributes, Shader::Binding::Node);

	Shader* unlitShader = new Shader();
	unlitShader->setUserCode(Utils::loadFile(DATA_DIR  "/unlit.wgsl"));
	unlitShader->addVertexInput("position", 0, VertexFormat::Float32x3);
	unlitShader->addVertexInput("normal", 1, VertexFormat::Float32x3);
	unlitShader->addVertexInput("color", 2, VertexFormat::Float32x3);
	unlitShader->addVertexInput("uv", 3, VertexFormat::Float32x2);

	unlitShader->addVertexOutput("color", 0, VertexFormat::Float32x3);
	unlitShader->addVertexOutput("normal", 1, VertexFormat::Float32x3);
	unlitShader->addVertexOutput("uv", 2, VertexFormat::Float32x2);
	//unlitShader->addVertexOutput("worldPosition", 3, VertexFormat::Float32x4);

	Issam::Attributed unlitMaterialAttributes;
	unlitMaterialAttributes.addAttribute("colorFactor", glm::vec4(1.0f));
	unlitMaterialAttributes.addAttribute("colorTexture", whiteTextureView);
	unlitMaterialAttributes.addAttribute("defaultSampler", defaultSampler);
	Issam::AttributedManager::getInstance().add(c_unlitMaterialAttributes, unlitMaterialAttributes);
	unlitShader->addAttributes(c_unlitMaterialAttributes, Shader::Binding::Material);

	Issam::Attributed unlitSceneAttributes;
	unlitSceneAttributes.addAttribute("view", mat4(1.0));
	unlitSceneAttributes.addAttribute("projection", mat4(1.0));
	Issam::AttributedManager::getInstance().add(Issam::c_unlitSceneAttributes, unlitSceneAttributes);
	unlitShader->addAttributes(Issam::c_unlitSceneAttributes, Shader::Binding::Scene);

	unlitShader->addAttributes(Issam::c_pbrNodeAttributes, Shader::Binding::Node); //le meme que PBR


	//TODO Remove 
	Shader* unlit2Shader = new Shader();
	unlit2Shader->setUserCode(Utils::loadFile(DATA_DIR  "/unlit.wgsl"));
	unlit2Shader->addVertexInput("position", 0, VertexFormat::Float32x3);
	unlit2Shader->addVertexInput("normal", 1, VertexFormat::Float32x3);
	unlit2Shader->addVertexInput("color", 2, VertexFormat::Float32x3);
	unlit2Shader->addVertexInput("uv", 3, VertexFormat::Float32x2);

	unlit2Shader->addVertexOutput("color", 0, VertexFormat::Float32x3);
	unlit2Shader->addVertexOutput("normal", 1, VertexFormat::Float32x3);
	unlit2Shader->addVertexOutput("uv", 2, VertexFormat::Float32x2);
	//unlitShader->addVertexOutput("worldPosition", 3, VertexFormat::Float32x4);

	Issam::Attributed unlit2MaterialAttributes;
	unlit2MaterialAttributes.addAttribute("colorFactor", glm::vec4(1.0f));
	unlit2MaterialAttributes.addAttribute("colorTexture", whiteTextureView);
	unlit2MaterialAttributes.addAttribute("defaultSampler", defaultSampler);
	Issam::AttributedManager::getInstance().add(c_unlit2MaterialAttributes, unlit2MaterialAttributes);
	unlit2Shader->addAttributes(c_unlit2MaterialAttributes, Shader::Binding::Material);
	unlit2Shader->addAttributes(Issam::c_unlitSceneAttributes, Shader::Binding::Scene);
	unlit2Shader->addAttributes(Issam::c_pbrNodeAttributes, Shader::Binding::Node);
	
	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
		if (m_drag.active) {
			vec2 currentMouse = vec2((float)xpos, (float)ypos);
			vec2 delta = (currentMouse - m_drag.startMouse) * m_drag.sensitivity;
			m_cameraState.angles = m_drag.startCameraState.angles + delta;
			// Clamp to avoid going too far when orbitting up/down
			m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
			// Inertia
			m_drag.velocity = delta - m_drag.previousDelta;
			m_drag.previousDelta = delta;
		}
	});

	glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse) {
			// Don't rotate the camera if the mouse is already captured by an ImGui
			// interaction at this frame.
			return;
		}
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			switch (action) {
			case GLFW_PRESS:
			{
				m_drag.active = true;
				double xpos, ypos;
				glfwGetCursorPos(window, &xpos, &ypos);
				m_drag.startMouse = vec2(-(float)xpos, (float)ypos);
				m_drag.startCameraState = m_cameraState;
				auto& camera = scene->getRegistry().get<Issam::Camera>(cameraEntity);

				glm::mat4 viewMatrix = camera.m_view;
				glm::mat4 projectionMatrix = camera.m_projection;
				glm::vec3 rayOrigin = glm::vec3(glm::inverse(viewMatrix) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
				glm::vec3 rayDirection = getRayFromMouse(m_winWidth - xpos, m_winHeight - ypos, viewMatrix, projectionMatrix, m_winWidth, m_winHeight);
				auto view = scene->getRegistry().view<const Issam::WorldTransform, Issam::Filters, Issam::MeshRenderer>();
				for (auto entity : view)
				{
					Issam::MeshRenderer meshRenderer = view.get<Issam::MeshRenderer>(entity);
					Mesh* mesh = MeshManager::getInstance().get(meshRenderer.meshId);
					if (mesh)
					{
						auto& transform = view.get<Issam::WorldTransform>(entity);

						glm::vec3 aabb_min = transform.getTransform() * glm::vec4(mesh->getBoundingBox().first, 1.0);
						glm::vec3 aabb_max = transform.getTransform() * glm::vec4(mesh->getBoundingBox().second, 1.0);
						BoundingBox	aabb = std::make_pair(aabb_min, aabb_max);
						float t;
						if (rayIntersectsBoundingBox(rayOrigin, rayDirection, aabb, t)) {
							if (pickedEntity != entt::null)
							{
								auto& filters = scene->getRegistry().get<Issam::Filters>(pickedEntity);
								filters.remove("unlit");
							}
								
							pickedEntity = entity;
							auto& filters = scene->getRegistry().get<Issam::Filters>(entity);
							filters.add("unlit");
							//	std::cout << "Ray intersects the bounding box at t = " << t << std::endl;
						}
						else {
							//	std::cout << "Ray does not intersect the bounding box" << std::endl;
						}
					}

				}
			}
				
				break;
			case GLFW_RELEASE:
				m_drag.active = false;
				break;
			}
		}
	});

	glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) {
		m_cameraState.zoom += m_drag.scrollSensitivity * static_cast<float>(yoffset);
		m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -20.0f, 20.0f);
	});

	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
	{
	});

	ImGUIWrapper* imgui = new ImGUIWrapper(window, swapChainFormat, depthTextureFormat); //After glfw callbacks
	
	scene = new Issam::Scene();

	TextureView depthBuffer = createBuffer(m_winWidth, m_winHeight, depthTextureFormat);
	TextureView colorBuffer = createBuffer(m_winWidth, m_winHeight, TextureFormat::BGRA8Unorm);
	TextureView tmpColorBuffer = createBuffer(m_winWidth, m_winHeight, TextureFormat::BGRA8Unorm);

	//Pass* backgroundPass = new Pass(backgroundShader);
	//Pipeline* backgroundPipeline = new Pipeline(backgroundShader, swapChainFormat, depthTextureFormat);
	//backgroundPass->setPipeline(backgroundPipeline);
	//backgroundPass->setType(Pass::Type::FILTER);
	//backgroundPass->setDepthBuffer(depthBuffer); //TODO Remove

	Pass* passPbr = new Pass(pbrShader);
	Pipeline* pipelinePbr = new Pipeline(pbrShader, swapChainFormat, depthTextureFormat, Pipeline::BlendingMode::Over);
	passPbr->setPipeline(pipelinePbr);
	passPbr->setDepthBuffer(depthBuffer);
	passPbr->addFilter("pbr");
	//passPbr->setClearColor(false);
	//passPbr->setImGuiWrapper(imgui);

	Pass* unlitPass = new Pass(unlitShader);
	Pipeline* pipelineUnlit = new Pipeline(unlitShader, swapChainFormat, depthTextureFormat, Pipeline::BlendingMode::Replace);
	unlitPass->setPipeline(pipelineUnlit);
	unlitPass->setDepthBuffer(depthBuffer);
	unlitPass->setColorBuffer(tmpColorBuffer);
	unlitPass->addFilter("unlit");
	unlitPass->setClearColor(true);
	unlitPass->setClearColorValue(Color(0.0,0.0,0.0,0.0));
	//unlitPass->setImGuiWrapper(imgui);

	//Pass* pbrPass2 = new Pass(pbrShader);
	////Pipeline* pipelineUnlit = new Pipeline(unlitShader, swapChainFormat, depthTextureFormat);
	//pbrPass2->setPipeline(pipelinePbr);
	//pbrPass2->setDepthBuffer(depthBuffer);
	//pbrPass2->setImGuiWrapper(imgui);
	//pbrPass2->addFilter("unlit");
	////unlitPass->addFilter("pbr");
	//pbrPass2->setClearColor(false);

	Pass* dilatationPass = new Pass(diltationShader);
	Pipeline* dilatationPipeline = new Pipeline(diltationShader, swapChainFormat, depthTextureFormat, Pipeline::BlendingMode::Replace);
	dilatationPass->setPipeline(dilatationPipeline);
	dilatationPass->setType(Pass::Type::FILTER);
	dilatationPass->setDepthBuffer(depthBuffer); //TODO dont need depthBuffer
	dilatationPass->setClearColor(false);
	dilatationPass->setColorBuffer(colorBuffer);
	//dilatationPass->setClearColorValue(Color(0.0, 0.0, 0.0, 0.0));
	scene->setAttribute("source", tmpColorBuffer);
	//scene->setAttribute("source", TextureManager::getInstance().getTextureView(jpgFiles[1]));

	Pass* unlit2Pass = new Pass(unlit2Shader);
	Pipeline* pipelineUnlit2 = new Pipeline(unlit2Shader, swapChainFormat, depthTextureFormat, Pipeline::BlendingMode::Replace);
	unlit2Pass->setPipeline(pipelineUnlit2);
	unlit2Pass->setDepthBuffer(depthBuffer);
	unlit2Pass->setColorBuffer(colorBuffer);
	unlit2Pass->addFilter("unlit");
	unlit2Pass->setClearColor(false);

	Pass* toScreenPass = new Pass(backgroundShader);
	Pipeline* backgroundPipeline = new Pipeline(backgroundShader, swapChainFormat, depthTextureFormat, Pipeline::BlendingMode::Over);
	toScreenPass->setPipeline(backgroundPipeline);
	toScreenPass->setType(Pass::Type::FILTER);
	toScreenPass->setClearColor(false);
	toScreenPass->setDepthBuffer(depthBuffer); //TODO dont need depthBuffer
	scene->setAttribute("backgroundTexture", colorBuffer);
	//scene->setAttribute("backgroundTexture", tmpColorBuffer);
	toScreenPass->setImGuiWrapper(imgui);

	vec3 focalPoint(0.0, 0.0, -2.0);
	float angle2 = 3.0f * PI / 4.0f;
	mat4x4 V(1.0);
	V = glm::translate(V, -focalPoint);
	V = glm::rotate(V, -angle2, vec3(1.0, 0.0, 0.0));

	float ratio = 640.0f / 480.0f;
	float focalLength = 2.0;
	float nearPlane = 0.01f;
	float farPlane = 300.0f;
	float fov = 2 * glm::atan(1 / focalLength);
	mat4x4 proj = glm::perspective(fov, ratio, nearPlane, farPlane);

	cameraEntity = scene->addCamera();
	auto& camera = scene->getRegistry().get<Issam::Camera>(cameraEntity);
	camera.m_pos = focalPoint;
	camera.m_view = V;
	camera.m_projection = proj;
	scene->getRegistry().patch<Issam::Camera>(cameraEntity);
	entt::entity lightEnitity = scene->addLight();
	auto& light = scene->getRegistry().get<Issam::Light>(lightEnitity);
	light.m_direction = vec3(0.5, -0.9, 0.1);
	scene->getRegistry().patch<Issam::Light>(lightEnitity);
	//scene->setAttribute("backgroundTexture", TextureManager::getInstance().getTextureView(jpgFiles[1]));

	Renderer renderer;
	//renderer.addPass(backgroundPass);
	renderer.addPass(passPbr);
	renderer.addPass(unlitPass);
	//renderer.addPass(pbrPass2);
 	renderer.addPass(dilatationPass);
	renderer.addPass(unlit2Pass); //to remove interior
	renderer.addPass(toScreenPass);
	renderer.setScene(scene);

	//Issam::Node* selectedNode = nullptr;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
	 	updateViewMatrix(scene); //TODO l'appeller qu'au besoin

		{
			imgui->begin();
			

			ImGui::Begin("Material Editor");
			{
				std::vector<std::string> gltfFiles = GetFiles("C:/Dev/glTF-Sample-Models/2.0", ".gltf");
				static int selectedGLTFIndex = -1;
				if (ImGui::BeginCombo("GLTF Files", selectedGLTFIndex == -1 ? "Select a GLTF" : gltfFiles[selectedGLTFIndex].c_str())) {
					for (int i = 0; i < gltfFiles.size(); i++) {
						bool isSelected = (selectedGLTFIndex == i);
						if (ImGui::Selectable(gltfFiles[i].c_str(), isSelected)) {
							if (selectedGLTFIndex != -1)
							{
								MeshManager::getInstance().clear();//MeshManager::getInstance().remove(gltfFiles[selectedGLTFIndex]);
								scene->clear();
								pickedEntity = entt::null;
							}
							selectedGLTFIndex = i;
							Utils::LoadGLTF(gltfFiles[i], scene);
							//scene->setNodes(nodes);

						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
			

			if (pickedEntity != entt::null && scene->getRegistry().valid(pickedEntity)/*&& pickedEntity->material*/)
			{
				Material* selectedMaterial = scene->getRegistry().get<Issam::MeshRenderer>(pickedEntity).material;

				//Material* selectedMaterial = pickedNode->material;
				glm::vec4 baseColorFactor = std::get<glm::vec4>(selectedMaterial->getAttribute("baseColorFactor").value);
				ImGui::ColorEdit4("BaseColorFactor", (float*)&baseColorFactor);
				selectedMaterial->setAttribute("baseColorFactor", baseColorFactor);
				{
					static int selectedTextureIndex = -1;
					std::vector<std::string> textureNames;
					for (const auto& texturePair : TextureManager::getInstance().getAll()) {
						textureNames.push_back(texturePair.first);
					}

					if (ImGui::BeginCombo("BaseColorTexture", selectedTextureIndex == -1 ? "Select a texture" : textureNames[selectedTextureIndex].c_str())) {
						for (int i = 0; i < textureNames.size(); i++) {
							bool isSelected = (selectedTextureIndex == i);
							if (ImGui::Selectable(textureNames[i].c_str(), isSelected)) {
								selectedTextureIndex = i;
								//	std::cout << "Selected texture: " << textureNames[i] << std::endl;
								selectedMaterial->setAttribute("baseColorTexture", TextureManager::getInstance().getTextureView(textureNames[i]));
								
								
							}
							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
				}
				float metallicFactor = std::get<float>(selectedMaterial->getAttribute("metallicFactor").value);
				ImGui::SliderFloat("MetallicFactor", &metallicFactor, 0.0 , 1.0);
				selectedMaterial->setAttribute("metallicFactor", metallicFactor);
				
				float roughnessFactor = std::get<float>(selectedMaterial->getAttribute("roughnessFactor").value);
				ImGui::SliderFloat("RoughnessFactor", &roughnessFactor, 0.0, 1.0);
				selectedMaterial->setAttribute("roughnessFactor", roughnessFactor);
			}

			static glm::vec3 lightDirection = glm::vec3(1.0);
			ImGui::SliderFloat3("lightDirection", (float*)&lightDirection, -1.0, 1.0);
			auto& light = scene->getRegistry().get<Issam::Light>(lightEnitity);
			light.m_direction = lightDirection;
			scene->getRegistry().patch<Issam::Light>(lightEnitity);
			
			/*static float translation[3] = { 0.0, 0.0, 0.0 };
			static float rotation[3] = { 0.0, 0.0, 0.0 };
			static float scale[3] = { 1.0, 1.0, 1.0 };
			ImGui::InputFloat3("Translation", translation);
			ImGui::InputFloat3("Rotation", rotation);
			ImGui::InputFloat3("Scale", scale);
			glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), glm::make_vec3(translation));
			glm::mat4 rotationMatrix = computeRotationMatrix(glm::make_vec3(rotation));
			glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::make_vec3(scale));
			glm::mat4 transformation = translationMatrix * rotationMatrix * scaleMatrix;
			shader->setUniform("model", transformation);*/

			ImGuiIO& io = ImGui::GetIO();
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();
		}

		renderer.draw();

	}

	Context::getInstance().shutdownGraphics();
	
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}