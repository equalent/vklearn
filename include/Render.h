#pragma once

#include "Engine.h"

#define VULKAN_HPP_NO_EXCEPTIONS
#include "vulkan/vulkan.hpp"

const vk::ApplicationInfo kRenderApplicationInfo = {
	"VkLearn",
	0,
	"VkLearnEngine",
	0,
	VK_API_VERSION_1_0
};

const uint32_t kVendorIdNvidia = 0x10de;
const uint32_t kVendorIdAmd = 0x1002;

class CRender
{
public:
	EEngineStatus Initialize();
	EEngineStatus Update();
	EEngineStatus Shutdown();
private:
	EEngineStatus LoadShadersTriangle();
	uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;
	
	vk::DispatchLoaderDynamic m_DispatchLoader;
	
	vk::Instance m_Instance;
#ifdef _DEBUG
	vk::DebugReportCallbackEXT m_DebugReportCallback;
#endif
	vk::PhysicalDevice m_PhysicalDevice;
	vk::Device m_Device;
	vk::Queue m_GraphicsQueue;
	vk::SurfaceKHR m_Surface;
	vk::Format m_SwapChainFormat = vk::Format::eUndefined;
	vk::Extent2D m_SwapChainExtent;
	vk::SwapchainKHR m_SwapChain;
	std::vector<vk::ImageView> m_SwapChainImageViews;
	vk::RenderPass m_RenderPass1;
	std::vector<vk::Framebuffer> m_SwapChainFrameBuffers;
	vk::CommandPool m_CommandPool;
	std::vector<vk::CommandBuffer> m_CommandBuffers;
	vk::PipelineLayout m_PipelineLayout;
	vk::Pipeline m_Pipeline;

	vk::Buffer m_VertexBuffer;
	vk::DeviceMemory m_VertexBufferMemory;

	/* SEMAPHORES */
	vk::Semaphore m_ImageAvailableSemaphore;
	vk::Semaphore m_RenderFinishedSemaphore;

	vk::ShaderModule m_TriangleVS;
	vk::ShaderModule m_TriangleFS;
};
