#pragma once

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "webgpu-utils.h"

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


	void initGraphics(GLFWwindow* window, uint16_t width, uint16_t height, WGPUTextureFormat swapChainFormat)
	{
		m_instance = createInstance(InstanceDescriptor{});
		if (!m_instance) {
			std::cerr << "Could not initialize WebGPU!" << std::endl;
			return;
		}

		//Requesting adapter...
		m_surface = glfwGetWGPUSurface(m_instance, window);
		RequestAdapterOptions adapterOpts;
		adapterOpts.compatibleSurface = m_surface;
		//adapterOpts.backendType = WGPUBackendType::WGPUBackendType_D3D12;
		adapterOpts.powerPreference = WGPUPowerPreference::WGPUPowerPreference_HighPerformance;
		m_adapter = m_instance.requestAdapter(adapterOpts);
		assert(m_adapter);
		//	inspectAdapter(m_adapter);

			//Requesting device...
		DeviceDescriptor deviceDesc;
		deviceDesc.label = "My Device";
		deviceDesc.requiredFeaturesCount = 0;
		deviceDesc.requiredLimits = nullptr;
		deviceDesc.defaultQueue.label = "The default queue";
		m_device = m_adapter.requestDevice(deviceDesc);
		assert(m_device);
		//	inspectDevice(m_device);

			// Add an error callback for more debug info
		m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
			std::cout << "Device error: type " << type;
			if (message) std::cout << " (message: " << message << ")";
			std::cout << std::endl;
		});

		auto h2 = m_device.setLoggingCallback([](LoggingType type, char const* message) {
			std::cout << "Device error: type " << type;
			if (message) std::cout << " (message: " << message << ")";
			std::cout << std::endl;
		});



		//Creating swapchain...
		SwapChainDescriptor swapChainDesc;
		swapChainDesc.width = width;
		swapChainDesc.height = height;
		swapChainDesc.usage = TextureUsage::RenderAttachment;
		swapChainDesc.format = swapChainFormat;
		swapChainDesc.presentMode = PresentMode::Fifo;
		m_swapChain = m_device.createSwapChain(m_surface, swapChainDesc);
		assert(m_swapChain);
	}

	std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;

	void shutdownGraphics()
	{
		m_swapChain.release();
		m_device.release();
		m_adapter.release();
		m_instance.release();
		m_surface.release();
	}

	Device getDevice() { return m_device; }
	SwapChain getSwapChain() { return m_swapChain; }

private:
	Context() = default;

	Device m_device = nullptr;
	SwapChain m_swapChain = nullptr;
	Instance m_instance = nullptr;
	Surface m_surface = nullptr;
	Adapter m_adapter = nullptr;
};