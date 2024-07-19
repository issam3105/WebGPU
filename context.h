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
	static Context& getInstance()
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
		RequestAdapterOptions adapterOpts;
		adapterOpts.compatibleSurface = nullptr;
		adapterOpts.backendType = BackendType::Vulkan;
		adapterOpts.powerPreference = PowerPreference::HighPerformance;

		m_adapter = RequestAdapter(m_instance, &adapterOpts);
		assert(m_adapter);
		//inspectAdapter(m_adapter.Get());

			//Requesting device...
		DeviceDescriptor deviceDesc;
		deviceDesc.label = "My Device";
	//	deviceDesc.requiredFeaturesCount = 0;
		deviceDesc.requiredLimits = nullptr;
		deviceDesc.defaultQueue.label = "The default queue";
		
		std::vector<FeatureName> requiredFeatures = {
		    FeatureName::Float32Filterable
		};

		deviceDesc.requiredFeatures = requiredFeatures.data();
		deviceDesc.requiredFeatureCount = static_cast<uint32_t>(requiredFeatures.size());

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

	//	//Creating swapchain...
		m_surface = glfw::CreateSurfaceForWindow(m_instance, window);
		SurfaceConfiguration config;
		config.device = m_device;
		config.format = swapChainFormat;
		config.usage = TextureUsage::RenderAttachment;
		config.width = static_cast<uint32_t>(width);
		config.height = static_cast<uint32_t>(height);
		config.presentMode = PresentMode::Fifo,
		m_surface.Configure(&config);
	}

	Surface getSurface()
	{
		return m_surface;
	}

	void shutdownGraphics()
	{
	}

	Device getDevice() { return m_device; }

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
	Instance m_instance = nullptr;
	Surface m_surface = nullptr;
	Adapter m_adapter = nullptr;
};
