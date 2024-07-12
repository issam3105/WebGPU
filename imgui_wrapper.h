#pragma once


#include "context.h"

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

class Wrapper {
public:
	virtual void draw(RenderPassEncoder renderPass) = 0;
};

class ImGUIWrapper : public Wrapper
{
public:
	ImGUIWrapper(GLFWwindow* window, TextureFormat swapChainFormat, TextureFormat depthTextureFormat)
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO();

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOther(window, true);
		//	ImGui_ImplGlfw_InstallCallbacks(window);
		ImGui_ImplWGPU_Init(Context::getInstance().getDevice().Get(), 3, static_cast<WGPUTextureFormat>(swapChainFormat), static_cast<WGPUTextureFormat>(depthTextureFormat));
	};
	~ImGUIWrapper()
	{
		ImGui_ImplGlfw_Shutdown();
		ImGui_ImplWGPU_Shutdown();
	};

	void begin()
	{
		// Start the Dear ImGui frame
		ImGui_ImplWGPU_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void draw(RenderPassEncoder renderPass) override
	{
		// Draw the UI
		ImGui::EndFrame();
		// Convert the UI defined above into low-level drawing commands
		ImGui::Render();
		// Execute the low-level drawing commands on the WebGPU backend
		ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass.Get());
	}
};