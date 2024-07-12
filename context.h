#pragma once

#include <GLFW/glfw3.h>
#include <webgpu/webgpu_glfw.h>
#include <webgpu/webgpu_cpp.h>

#include "webgpu-utils.h"
#include <iostream>

using namespace wgpu;

class Context
{
public:
	static Context& Context::getInstance()
	{
		static Context ctx;
		return ctx;
	}

	~Context() = default;


	void initGraphics(GLFWwindow* window, uint16_t width, uint16_t height, TextureFormat swapChainFormat)
	{
		m_instance = CreateInstance();
		if (!m_instance) {
			std::cerr << "Could not initialize WebGPU!" << std::endl;
			return;
		}

		//Requesting adapter...
		m_surface = glfw::CreateSurfaceForWindow(m_instance, window);
		RequestAdapterOptions adapterOpts;
		adapterOpts.compatibleSurface = m_surface;
		adapterOpts.backendType = BackendType::Vulkan;
		adapterOpts.powerPreference = PowerPreference::HighPerformance;

		m_adapter = RequestAdapter(m_instance, &adapterOpts);
		assert(m_adapter);
		//	inspectAdapter(m_adapter);

			//Requesting device...
		DeviceDescriptor deviceDesc;
		deviceDesc.label = "My Device";
	//	deviceDesc.requiredFeaturesCount = 0;
		deviceDesc.requiredLimits = nullptr;
		deviceDesc.defaultQueue.label = "The default queue";
		m_device = RequestDevice(m_adapter, &deviceDesc);
		//m_device = m_adapter.requestDevice(deviceDesc);
		assert(m_device);
		//	inspectDevice(m_device);

			// Add an error callback for more debug info
		auto onUncapturedError = [](WGPUErrorType type, char const* message, void* userdata) {
			std::cout << "Device error: type " << type;
			if (message) std::cout << " (message: " << message << ")";
			std::cout << std::endl;
		};

		m_device.SetUncapturedErrorCallback(onUncapturedError, nullptr);

		//Creating swapchain...
		SwapChainDescriptor swapChainDesc;
		swapChainDesc.width = width;
		swapChainDesc.height = height;
		swapChainDesc.usage = TextureUsage::RenderAttachment;
		swapChainDesc.format = swapChainFormat;
		swapChainDesc.presentMode = PresentMode::Fifo; //Immediate
		m_swapChain = m_device.CreateSwapChain(m_surface, &swapChainDesc);
		assert(m_swapChain);
	}

	void shutdownGraphics()
	{
	}

	Device getDevice() { return m_device; }
	SwapChain getSwapChain() { return m_swapChain; }

private:
	Device RequestDevice(Adapter& instance, DeviceDescriptor const* descriptor) {
		struct UserData {
			WGPUDevice device = nullptr;
			bool requestEnded = false;
		};
		UserData userData;

		auto onDeviceRequestEnded = [](
			WGPURequestDeviceStatus status, WGPUDevice device,
			char const* message, void* pUserData
			) {
			UserData& userData = *reinterpret_cast<UserData*>(pUserData);
			if (status == WGPURequestDeviceStatus_Success) {
				userData.device = device;
			}
			else {
				std::cout << "Could not get WebGPU adapter: " << message << std::endl;
			}
			userData.requestEnded = true;
		};

		instance.RequestDevice(descriptor, onDeviceRequestEnded, &userData);

		// assert(userData.requestEnded);

		return userData.device;
	}

	Adapter RequestAdapter(Instance& instance, RequestAdapterOptions const* options) {
		struct UserData {
			WGPUAdapter adapter = nullptr;
			bool requestEnded = false;
		};
		UserData userData;

		auto onAdapterRequestEnded = [](
			WGPURequestAdapterStatus status, WGPUAdapter adapter,
			char const* message, void* pUserData
			) {
			UserData& userData = *reinterpret_cast<UserData*>(pUserData);
			if (status == WGPURequestAdapterStatus_Success) {
				userData.adapter = adapter;
			}
			else {
				std::cout << "Could not get WebGPU adapter: " << message << std::endl;
			}
			userData.requestEnded = true;
		};

		instance.RequestAdapter(options, onAdapterRequestEnded, &userData);

		// assert(userData.requestEnded);

		return userData.adapter;
	}
	Context() = default;

	Device m_device = nullptr;
	SwapChain m_swapChain = nullptr;
	Instance m_instance = nullptr;
	Surface m_surface = nullptr;
	Adapter m_adapter = nullptr;
};