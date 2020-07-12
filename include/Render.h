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
	EEngineStatus Update(float deltaTime);
	EEngineStatus Shutdown();
	const char* GetGpuName() const;

	float m_RotationSpeed = 5.f;
private:
	bool m_ShowDemoWindow = true;
	float m_ActualRotationSpeed = m_RotationSpeed;
	float m_Angle = 0.f;
	std::string m_GpuName;
	
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
	vk::RenderPass m_RenderPassImGui;
	std::vector<vk::Framebuffer> m_SwapChainFrameBuffers;
	vk::CommandPool m_CommandPool;
	std::vector<vk::CommandBuffer> m_CommandBuffers;
	std::vector<vk::CommandBuffer> m_CommandBuffersImGui;
	vk::PipelineLayout m_PipelineLayout;
	vk::Pipeline m_Pipeline;

	vk::Buffer m_UniformBuffer;
	vk::DeviceMemory m_UniformBufferMemory;
	vk::DescriptorPool m_DescriptorPool;
	vk::DescriptorSet m_DescriptorSet;
	vk::DescriptorSetLayout m_DescriptorSetLayout;

	vk::Buffer m_VertexBuffer;
	vk::DeviceMemory m_VertexBufferMemory;
	vk::Buffer m_IndexBuffer;
	vk::DeviceMemory m_IndexBufferMemory;

	/* SEMAPHORES */
	vk::Semaphore m_ImageAvailableSemaphore;
	vk::Semaphore m_RenderFinished1Semaphore;
	vk::Semaphore m_RenderFinished2Semaphore;

	vk::ShaderModule m_TriangleVS;
	vk::ShaderModule m_TriangleFS;
};
