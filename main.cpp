#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define TINYOBJLOADER_IMPLEMENTATION // add this to exactly 1 of your C++ files
#include "tiny_obj_loader.h"


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

void updateViewMatrix(Issam::Camera* camera) {
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	mat4x4 viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1));
	camera->setView(viewMatrix);
	camera->setPosition(vec4(position,0.0));
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
    
	std::string userCode = Utils::loadFile(DATA_DIR  "/sahder_1.wgsl");
	Shader* shader_1 = new Shader();
	shader_1->setUserCode(userCode);
	shader_1->addVertexInput("position", 0, VertexFormat::Float32x3);
	shader_1->addVertexInput("normal", 1, VertexFormat::Float32x3);
	shader_1->addVertexInput("color", 2, VertexFormat::Float32x3);
	shader_1->addVertexInput("uv", 3, VertexFormat::Float32x2);

	shader_1->addVertexOutput("color", 0, VertexFormat::Float32x3);
	shader_1->addVertexOutput("normal", 1, VertexFormat::Float32x3);
	shader_1->addVertexOutput("uv", 2, VertexFormat::Float32x2);

	shader_1->addVertexOutput("worldPosition", 3, VertexFormat::Float32x4);

	MaterialModule* materialPbrModule = new MaterialModule();

	materialPbrModule->addUniform("baseColorFactor", glm::vec4(1.0f));
	materialPbrModule->addUniform("metallicFactor", 0.5f);
	materialPbrModule->addUniform("roughnessFactor", 0.5f);

	materialPbrModule->addTexture("baseColorTexture", whiteTextureView);
	materialPbrModule->addTexture("metallicRoughnessTexture", whiteTextureView);
	materialPbrModule->addSampler("defaultSampler", defaultSampler);

	shader_1->setMaterialModule(materialPbrModule);

	MaterialModuleManager::getInstance().add("pbrMat", *materialPbrModule);

//	ShaderManager::getInstance().add("shader1", shader_1);
	
	
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
				m_drag.active = true;
				double xpos, ypos;
				glfwGetCursorPos(window, &xpos, &ypos);
				m_drag.startMouse = vec2(-(float)xpos, (float)ypos);
				m_drag.startCameraState = m_cameraState;
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
	//	auto shader = ShaderManager::getInstance().getShader("shader1");
	/*	if (key == GLFW_KEY_E && action == GLFW_PRESS)
			material->setTexture("baseColorTexture", TextureManager::getInstance().getTextureView("whiteTex"));
		if (key == GLFW_KEY_R && action == GLFW_PRESS)
			material->setTexture("baseColorTexture", TextureManager::getInstance().getTextureView("fourareen2K_albedo"));
		if(key == GLFW_KEY_D && action == GLFW_PRESS)
			material->setUniform("baseColorFactor", glm::vec4(1.0f));
		if (key == GLFW_KEY_F && action == GLFW_PRESS)
			material->setUniform("baseColorFactor", glm::vec4(1.0f, 0.0, 0.0, 1.0));*/
	});

	ImGUIWrapper* imgui = new ImGUIWrapper(window, swapChainFormat, depthTextureFormat); //After glfw callbacks

	Pass pass1(shader_1);
	Pipeline* pipeline = new Pipeline(shader_1, swapChainFormat, depthTextureFormat);
	pass1.setPipeline(pipeline);
	pass1.addDepthBuffer(m_winWidth, m_winHeight, depthTextureFormat);
	pass1.setImGuiWrapper(imgui);

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
	Issam::Camera* camera = new Issam::Camera;
	camera->setPosition(vec4(focalPoint, 0.0));
	camera->setView(V);
	camera->setProjection(proj);
	camera->setLightDirection(vec4(vec3(0.5, -0.9, 0.1),0.0));

	Renderer renderer;
	renderer.addPass(pass1);
	//renderer.setCamera(camera);
	Issam::Scene* scene = new Issam::Scene();
	scene->setCamera(camera);
	renderer.setScene(scene);

	Issam::Node* selectedNode = nullptr;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
	 	updateViewMatrix(camera);

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
								MeshManager::getInstance().clear();//MeshManager::getInstance().remove(gltfFiles[selectedGLTFIndex]);
							selectedGLTFIndex = i;
							std::vector<Issam::Node*> nodes = Utils::LoadGLTF(gltfFiles[i]);
							scene->setNodes(nodes);

						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
			
			{
				std::vector<std::string> nodesNames;
				for (auto node : scene->getNodes()) {
					nodesNames.push_back(node->name);
				}
				static int nodeIndex = -1;

				if (ImGui::BeginCombo("Nodes", nodeIndex == -1 ? "Select a Node" : nodesNames[nodeIndex].c_str())) {
					for (int i = 0; i < nodesNames.size(); i++) {
						bool isSelected = (nodeIndex == i);
						if (ImGui::Selectable(nodesNames[i].c_str(), isSelected)) {

							nodeIndex = i;
							selectedNode = scene->getNodes()[nodeIndex];
							std::cout << nodesNames[i].c_str() << " selected !" << std::endl;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}

			if (selectedNode && selectedNode->material)
			{
				Material* selectedMaterial = selectedNode->material;
				glm::vec4 baseColorFactor = std::get<glm::vec4>(selectedMaterial->getUniform("baseColorFactor").value);
				ImGui::ColorEdit4("BaseColorFactor", (float*)&baseColorFactor);
				selectedMaterial->setUniform("baseColorFactor", baseColorFactor);
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
								selectedMaterial->setTexture("baseColorTexture", TextureManager::getInstance().getTextureView(textureNames[i]));
							}
							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
				}

				
			}

			static glm::vec4 lightDirection = glm::vec4(1.0);
			ImGui::SliderFloat3("lightDirection", (float*)&lightDirection, -100.0, 100.0);
			camera->setLightDirection(lightDirection);
			
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