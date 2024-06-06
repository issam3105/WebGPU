/**
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://eliemichel.github.io/LearnWebGPU
 *
 * MIT License
 * Copyright (c) 2022-2023 Elie Michel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#define TINYOBJLOADER_IMPLEMENTATION // add this to exactly 1 of your C++ files
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

void updateViewMatrix(Shader* shader) {
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	mat4x4 viewMatrix = glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1));
	shader->setUniform("view", viewMatrix);

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

	//addTwoTriangles();
	//addColoredPlane();
	//addPyramid();
	Utils::loadGeometryFromObj(DATA_DIR  "/fourareen.obj");

	TextureView textureView = nullptr;
	Texture texture = Utils::loadTexture(DATA_DIR "/fourareen2K_albedo.jpg", &textureView);
	TextureManager().getInstance().add("fourareen2K_albedo", &textureView);

	TextureView whiteTextureView = nullptr;
	Texture  whiteTexture = Utils::CreateWhiteTexture( &whiteTextureView);
	TextureManager().getInstance().add("whiteTex", &whiteTextureView);

	
	TextureView uv_checkerTextureView = nullptr;
	Texture uv_checkerTexture = Utils::loadTexture(DATA_DIR "/uv_checker.jpg", &uv_checkerTextureView);
	TextureManager().getInstance().add("uv_checker", &uv_checkerTextureView);

	// Create a sampler

	Sampler defaultSampler = Utils::createDefaultSampler();
    
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

	shader_1->addUniform("baseColorFactor", glm::vec4(1.0f));
	shader_1->addUniform("time", 0.0f); 
    shader_1->addUniform("projection", glm::mat4x4(1.0f));
    shader_1->addUniform("view", glm::mat4x4(1.0f));
	shader_1->addUniform("model", glm::mat4x4(1.0f));

	shader_1->addTexture("baseColorTexture", whiteTextureView);
	shader_1->addSampler("defaultSampler", defaultSampler);

	ShaderManager::getInstance().add("shader1", shader_1);
	
	vec3 focalPoint(0.0, 0.0, -2.0);
	float angle2 = 3.0f * PI / 4.0f;
	mat4x4 V(1.0);
	V = glm::translate(V, -focalPoint);
	V = glm::rotate(V, -angle2, vec3(1.0, 0.0, 0.0));
	shader_1->setUniform("view", V);

	float ratio = 640.0f / 480.0f;
	float focalLength = 2.0;
	float near = 0.01f;
	float far = 100.0f;
	float fov = 2 * glm::atan(1 / focalLength);
	mat4x4 proj = glm::perspective(fov, ratio, near, far);
	shader_1->setUniform("projection", proj);
	shader_1->setUniform("model", glm::mat4(1.0f));

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
		m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -2.0f, 2.0f);
	});

	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto shader = ShaderManager::getInstance().getShader("shader1");
		if (key == GLFW_KEY_E && action == GLFW_PRESS)
			shader->setTexture("baseColorTexture", *TextureManager::getInstance().getTextureView("whiteTex"));
		if (key == GLFW_KEY_R && action == GLFW_PRESS)
			shader->setTexture("baseColorTexture", *TextureManager::getInstance().getTextureView("fourareen2K_albedo"));
		if(key == GLFW_KEY_D && action == GLFW_PRESS)
			shader->setUniform("baseColorFactor", glm::vec4(1.0f));
		if (key == GLFW_KEY_F && action == GLFW_PRESS)
			shader->setUniform("baseColorFactor", glm::vec4(1.0f, 0.0, 0.0, 1.0));
	});

	ImGUIWrapper* imgui = new ImGUIWrapper(window, swapChainFormat, depthTextureFormat); //After glfw callbacks

	Pass pass1(shader_1);
	pass1.addDepthBuffer(m_winWidth, m_winHeight, depthTextureFormat);
	pass1.setImGuiWrapper(imgui);

	Renderer renderer(swapChainFormat, depthTextureFormat);
	renderer.addPass(pass1);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
	 	updateViewMatrix(shader_1);

		{
			imgui->begin();
			static ImVec4 clear_color = ImVec4(1.0f, 1.0f, 1.0f, 1.00f);

			ImGui::Begin("Material Editor");
			ImGui::ColorEdit4("BaseColorFactor", (float*)&clear_color);
			auto shader = ShaderManager::getInstance().getShader("shader1");
			shader->setUniform("baseColorFactor", glm::vec4(clear_color.x, clear_color.y, clear_color.z, clear_color.w));

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
						shader->setTexture("baseColorTexture", *TextureManager::getInstance().getTextureView(textureNames[i]));
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

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