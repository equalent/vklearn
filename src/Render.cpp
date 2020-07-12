#include "Render.h"



#include <chrono>
#include <fstream>
#include <gsl/gsl_util>

#include "Viewport.h"
#include "SDL_vulkan.h"
#include <glm/glm.hpp>


#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

#ifdef _DEBUG
VkBool32 VKAPI_CALL RenderDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	SDL_Log("[VULKAN] %s", pMessage);
	return VK_FALSE;
}
#endif

#define VKR(res) if(res != vk::Result::eSuccess){return EEngineStatus::Failed;}

template <typename T>
T ClampValue(const T& n, const T& lower, const T& upper) {
	return std::max(lower, std::min(n, upper));
}


struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Color;
};

struct UniBuffer
{
	float Angles;
	float RotationSpeed;
};

const Vertex kVertexData[] = {
	{
		{-0.5f, 0.5f, 0.f},
		{0.f, 0.f, 1.f}
	},
	{
		{0.f, -0.5f, 0.f},
		{0.f, 1.f, 0.f}
	},
	{
		{0.5f, 0.5f, 0.f},
		{1.f, 0.f, 0.f}
	},
};

const uint32_t kIndexData[] = {
	2,1,0
};


EEngineStatus CRender::Initialize()
{
	vk::Result vkResult;

	uint32_t extensionCount;
	if (SDL_Vulkan_GetInstanceExtensions(gEngine->GetViewport()->GetWindow(), &extensionCount, nullptr) != SDL_TRUE)
	{
		return EEngineStatus::Failed;
	}

#ifdef _DEBUG
	const size_t additionalExtensionCount = 1;
	const uint32_t layerCount = 1;
	const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
#else
	const size_t additionalExtensionCount = 0;
	const uint32_t layerCount = 0;
	const char** layers = nullptr;
#endif

	const char** extensions = new const char* [extensionCount + additionalExtensionCount];
	auto finalExtensions = gsl::finally([&]
		{
			delete[] extensions;
		});


#ifdef _DEBUG
	extensions[0] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
#endif

	if (SDL_Vulkan_GetInstanceExtensions(gEngine->GetViewport()->GetWindow(), &extensionCount, extensions + additionalExtensionCount) != SDL_TRUE)
	{
		return EEngineStatus::Failed;
	}

	extensionCount += additionalExtensionCount;

	const vk::InstanceCreateInfo instanceCreateInfo = {
		{},
		&kRenderApplicationInfo,
		layerCount,
		layers,
		extensionCount,
		extensions
	};
	/* vk::UniqueHandle<vk::Instance, vk::DispatchLoaderStatic> */
	std::tie(vkResult, m_Instance) = vk::createInstance(instanceCreateInfo);

	if (vkResult != vk::Result::eSuccess)
	{
		return EEngineStatus::Failed;
	}

#ifdef _DEBUG
	const vk::DebugReportCallbackCreateInfoEXT reportCallbackCreateInfo = {
		vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eInformation | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning,
		&RenderDebugReportCallback,
		this
	};
#endif

	m_DispatchLoader.init(m_Instance, vkGetInstanceProcAddr);

#ifdef _DEBUG
	// setting up the debug callback

	std::tie(vkResult, m_DebugReportCallback) = m_Instance.createDebugReportCallbackEXT(reportCallbackCreateInfo, nullptr, m_DispatchLoader);

	if (vkResult != vk::Result::eSuccess)
	{
		return EEngineStatus::Failed;
	}

	m_Instance.debugReportMessageEXT(vk::DebugReportFlagBitsEXT::eInformation, vk::DebugReportObjectTypeEXT::eUnknown, VK_NULL_HANDLE, 0, 0, "", "Test message", m_DispatchLoader);
#endif

	// selecting a suitable physical device

	std::vector<vk::PhysicalDevice> physicalDevices;
	std::tie(vkResult, physicalDevices) = m_Instance.enumeratePhysicalDevices();

	if (vkResult != vk::Result::eSuccess)
	{
		return EEngineStatus::Failed;
	}

	if (physicalDevices.empty())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "No physical devices present!", gEngine->GetViewport()->GetWindow());
		return EEngineStatus::Failed;
	}

	vk::PhysicalDevice selPhysicalDevice = nullptr;
	uint32_t selGraphicsFamily = 0;

	for (vk::PhysicalDevice& physicalDevice : physicalDevices)
	{
		const vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
		const char* vendorName;
		switch (properties.vendorID)
		{
		case kVendorIdNvidia:
			vendorName = "NVIDIA ";
			break;
		case kVendorIdAmd:
			vendorName = "AMD ";
			break;
		default:
			vendorName = "";
			break;
		}
		SDL_Log("[CRender] Found physical device: #%x (%s%s)", properties.deviceID, vendorName, properties.deviceName);
		m_GpuName = std::string(vendorName) + std::string(properties.deviceName);

		// checking suitability
		bool suitable = true;

		suitable = suitable && ((properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) || (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu));

		std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();

		uint32_t nGraphics = 0;
		uint32_t idxGraphics = 0;
		uint32_t nCompute = 0;
		// uint32_t idxCompute;
		uint32_t nTransfer = 0;
		// uint32_t idxTransfer;
		// uint32_t nPresent = 0;

		uint32_t idx = 0;
		for (auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
			{
				nGraphics++;
				idxGraphics = idx;
			}

			if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute)
			{
				nCompute++;
				// idxCompute = idx;
			}

			if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer)
			{
				nTransfer++;
				// idxTransfer = idx;
			}

			idx++;
		}

		suitable = suitable && (nGraphics >= 1);

		if (suitable)
		{
			selPhysicalDevice = physicalDevice;
			selGraphicsFamily = idxGraphics;
			break;
		}
	}

	if (!selPhysicalDevice)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "No compatible device found!", gEngine->GetViewport()->GetWindow());
		return EEngineStatus::Failed;
	}
	m_PhysicalDevice = selPhysicalDevice;

	SDL_Log("[CRender] Selected physical device: #%x", selPhysicalDevice.getProperties().deviceID);

	// creating a logical device

	// S1: device queue info
	float queuePriority = 1.0f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo = {
		{},
		selGraphicsFamily,
		1,
		&queuePriority
	};

	const char* deviceExtArray[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	const vk::DeviceCreateInfo deviceCreateInfo = {
		{},
		1,
		&deviceQueueCreateInfo,
		0,
		nullptr,
		1,
		deviceExtArray,
		{}
	};

	std::tie(vkResult, m_Device) = selPhysicalDevice.createDevice(deviceCreateInfo);

	if (vkResult != vk::Result::eSuccess)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "Unable to create VkDevice!", gEngine->GetViewport()->GetWindow());
		return EEngineStatus::Failed;
	}

	m_GraphicsQueue = m_Device.getQueue(selGraphicsFamily, 0);

	VkSurfaceKHR tempSurface;
	const SDL_bool sdlRes = SDL_Vulkan_CreateSurface(gEngine->GetViewport()->GetWindow(), m_Instance, &tempSurface);
	if (sdlRes != SDL_TRUE)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "Unable to create VkSurfaceKHR!", gEngine->GetViewport()->GetWindow());
		return EEngineStatus::Failed;
	}
	m_Surface = tempSurface;

	// check presentation support
	if (!(selPhysicalDevice.getSurfaceSupportKHR(selGraphicsFamily, m_Surface, m_DispatchLoader).value))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "VkSurfaceKHR does not support presentation!", gEngine->GetViewport()->GetWindow());
		return EEngineStatus::Failed;
	}

	// query swap chain support data
	SDL_Log("[CRender] Querying swap chain support data...");
	vk::SurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<vk::SurfaceFormatKHR> surfaceFormats;
	std::vector<vk::PresentModeKHR> presentModes;

	vk::SurfaceFormatKHR selSurfaceFormat;
	bool surfaceFormatFound = false;
	vk::PresentModeKHR selPresentMode;
	bool presentModeFound = false;

	std::tie(vkResult, surfaceCapabilities) = selPhysicalDevice.getSurfaceCapabilitiesKHR(m_Surface, m_DispatchLoader);
	VKR(vkResult)
		std::tie(vkResult, surfaceFormats) = selPhysicalDevice.getSurfaceFormatsKHR(m_Surface, m_DispatchLoader);
	VKR(vkResult)
		std::tie(vkResult, presentModes) = selPhysicalDevice.getSurfacePresentModesKHR(m_Surface, m_DispatchLoader);
	VKR(vkResult)

		for (vk::SurfaceFormatKHR surfaceFormat : surfaceFormats)
		{
			SDL_Log("[CRender] Surface format available: %s (%s)", vk::to_string(surfaceFormat.format).c_str(), vk::to_string(surfaceFormat.colorSpace).c_str());
			if (surfaceFormat.format == vk::Format::eB8G8R8A8Srgb && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			{
				selSurfaceFormat = surfaceFormat;
				surfaceFormatFound = true;
				break;
			}
		}

	if (!surfaceFormatFound)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "No suitable surface format available!", gEngine->GetViewport()->GetWindow());
		return EEngineStatus::Failed;
	}

	for (vk::PresentModeKHR presentMode : presentModes)
	{
		SDL_Log("[CRender] Present mode available: %s", vk::to_string(presentMode).c_str());
		if (presentMode == vk::PresentModeKHR::eImmediate)
		{
			selPresentMode = presentMode;
			presentModeFound = true;
			break;
		}
	}

	if (!presentModeFound)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "No suitable present mode available!", gEngine->GetViewport()->GetWindow());
		return EEngineStatus::Failed;
	}

	// choosing swap extent
	vk::Extent2D selSwapExtent;

	if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
	{
		selSwapExtent = surfaceCapabilities.currentExtent;
	}
	else
	{
		uint32_t width, height;
		SDL_Vulkan_GetDrawableSize(gEngine->GetViewport()->GetWindow(), reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));
		selSwapExtent.width = ClampValue(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		selSwapExtent.height = ClampValue(width, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	}

	// creating the swap chain
	vk::SwapchainCreateInfoKHR swapChainCreateInfo = {
		{},
		m_Surface,
		surfaceCapabilities.minImageCount + 1,
		selSurfaceFormat.format,
		selSurfaceFormat.colorSpace,
		selSwapExtent,
		1,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
		vk::SharingMode::eExclusive,
		0,
		nullptr,
		surfaceCapabilities.currentTransform,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		selPresentMode,
		true,
		nullptr
	};

	std::tie(vkResult, m_SwapChain) = m_Device.createSwapchainKHR(swapChainCreateInfo, nullptr, m_DispatchLoader);
	VKR(vkResult);

	std::vector<vk::Image> swapChainImages;
	std::tie(vkResult, swapChainImages) = m_Device.getSwapchainImagesKHR(m_SwapChain, m_DispatchLoader);
	VKR(vkResult);

	m_SwapChainExtent = selSwapExtent;
	m_SwapChainFormat = selSurfaceFormat.format;

	m_SwapChainImageViews.resize(swapChainImages.size());
	m_SwapChainFrameBuffers.resize(swapChainImages.size());

	uint32_t i = 0;
	for (auto& swapChainImage : swapChainImages)
	{
		vk::ImageViewCreateInfo createInfo = {
			{},
			swapChainImage,
			vk::ImageViewType::e2D,
			m_SwapChainFormat,
			{
				vk::ComponentSwizzle::eIdentity,
				vk::ComponentSwizzle::eIdentity,
				vk::ComponentSwizzle::eIdentity,
				vk::ComponentSwizzle::eIdentity
			},
			{
				vk::ImageAspectFlagBits::eColor,
				0,
				1,
				0,
				1
			}
		};

		std::tie(vkResult, m_SwapChainImageViews[i]) = m_Device.createImageView(createInfo);
		VKR(vkResult);
		i++;
	}

	{
		// creating the first render pass
		vk::AttachmentDescription colorAttachment = {
			{},
			m_SwapChainFormat,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		};

		vk::AttachmentReference colorAttachmentRef = {
			0,
			vk::ImageLayout::eColorAttachmentOptimal
		};

		vk::SubpassDescription subPassDesc = {
			{},
			vk::PipelineBindPoint::eGraphics,
			0,
			nullptr,
			1,
			&colorAttachmentRef
		};

		vk::SubpassDependency dependency = {
			0,
			VK_SUBPASS_EXTERNAL,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::AccessFlagBits::eColorAttachmentWrite,
			{},
			vk::DependencyFlagBits::eByRegion
		};

		vk::RenderPassCreateInfo renderPassCreateInfo = {
			{},
			1,
			&colorAttachment,
			1,
			&subPassDesc,
			1,
			&dependency
		};

		std::tie(vkResult, m_RenderPass1) = m_Device.createRenderPass(renderPassCreateInfo);
		VKR(vkResult);
	}

	{
		// creating the ImGui render pass
		vk::AttachmentDescription colorAttachment = {
			{},
			m_SwapChainFormat,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eLoad,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		};

		vk::AttachmentReference colorAttachmentRef = {
			0,
			vk::ImageLayout::eColorAttachmentOptimal
		};

		vk::SubpassDescription subPassDesc = {
			{},
			vk::PipelineBindPoint::eGraphics,
			0,
			nullptr,
			1,
			&colorAttachmentRef
		};

		vk::SubpassDependency dependency = {
			0,
			VK_SUBPASS_EXTERNAL,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::AccessFlagBits::eColorAttachmentWrite,
			{},
			vk::DependencyFlagBits::eByRegion
		};

		vk::RenderPassCreateInfo renderPassCreateInfo = {
			{},
			1,
			&colorAttachment,
			1,
			&subPassDesc,
			1,
			&dependency
		};

		std::tie(vkResult, m_RenderPassImGui) = m_Device.createRenderPass(renderPassCreateInfo);
		VKR(vkResult);
	}


	i = 0;

	// creating the framebuffers
	for (vk::ImageView& swapChainImageView : m_SwapChainImageViews)
	{
		vk::ImageView attachments[] = {
			m_SwapChainImageViews[i]
		};

		vk::FramebufferCreateInfo frameBufferCreateInfo = {
			{},
			m_RenderPass1,
			1,
			attachments,
			m_SwapChainExtent.width,
			m_SwapChainExtent.height,
			1
		};

		std::tie(vkResult, m_SwapChainFrameBuffers[i]) = m_Device.createFramebuffer(frameBufferCreateInfo);
		VKR(vkResult);

		i++;
	}

	// creating the uniform buffer
	vk::BufferCreateInfo uniformBufferCreateInfo = {
		{},
		sizeof(UniBuffer),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::SharingMode::eExclusive,
		1,
		&selGraphicsFamily
	};

	std::tie(vkResult, m_UniformBuffer) = m_Device.createBuffer(uniformBufferCreateInfo);

	vk::MemoryRequirements memoryRequirements;
	memoryRequirements = m_Device.getBufferMemoryRequirements(m_UniformBuffer);

	vk::MemoryAllocateInfo uniformBufferAllocInfo = {
		memoryRequirements.size,
		FindMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
	};

	std::tie(vkResult, m_UniformBufferMemory) = m_Device.allocateMemory(uniformBufferAllocInfo);
	vkResult = m_Device.bindBufferMemory(m_UniformBuffer, m_UniformBufferMemory, 0);

	// loading the shaders
	if (LoadShadersTriangle() != EEngineStatus::Ok)
	{
		return EEngineStatus::Failed;
	}

	// creating the pipeline

	// S1: IA
	vk::VertexInputBindingDescription bindingDesc[] = {
		{
		0, // binding number
		24, // sizeof(vec3) * 2 (see triangle.vert)
		vk::VertexInputRate::eVertex
		}
	};

	vk::VertexInputAttributeDescription attrDesc[] = {
		// in vec3 inPosition
		{
		0,
		0,
		vk::Format::eR32G32B32Sfloat,
		0
		},
		// in vec3 inColor
		{
		1,
		0,
		vk::Format::eR32G32B32Sfloat,
		12
		}
	};

	vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
		{},
		1,
		bindingDesc,
		2,
		attrDesc
	};

	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
		{},
		vk::PrimitiveTopology::eTriangleList,
		false
	};

	// S2: RS
	vk::Viewport viewport = {
		0,
		0,
		static_cast<float>(m_SwapChainExtent.width),
		static_cast<float>(m_SwapChainExtent.height),
		0.f,
		1.f
	};

	vk::Rect2D scissor = {
		{0, 0},
		m_SwapChainExtent
	};

	vk::PipelineViewportStateCreateInfo viewportStateCreateInfo = {
		{},
		1,
		&viewport,
		1,
		&scissor
	};

	vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
		{},
		false,
		false,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack,
		vk::FrontFace::eCounterClockwise,
		false,
		0,
		0,
		0,
		1.f
	};

	vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
		{},
		vk::SampleCountFlagBits::e1,
		false
	};

	vk::PipelineColorBlendAttachmentState colorBlendAttachmentState = {
		false,
		vk::BlendFactor::eSrcAlpha,
		vk::BlendFactor::eOneMinusSrcAlpha,
		vk::BlendOp::eAdd,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA // !!!!!!!!!!!!!!!!!IMPORTANT!!!!!!!!!!!!!!!!!!
	};

	vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
		{},
		false,
		vk::LogicOp::eCopy, // don't mind the op
		1,
		&colorBlendAttachmentState
	};

	// S3: UB

	vk::DescriptorSetLayoutBinding layoutBinding = {
		0,
		vk::DescriptorType::eUniformBuffer,
		1,
		vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex,
		nullptr
	};

	vk::DescriptorSetLayoutCreateInfo layoutCreateInfo = {
		{},
		1,
		&layoutBinding
	};

	std::tie(vkResult, m_DescriptorSetLayout) = m_Device.createDescriptorSetLayout(layoutCreateInfo);

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		{},
		1,
		&m_DescriptorSetLayout
	};

	std::tie(vkResult, m_PipelineLayout) = m_Device.createPipelineLayout(pipelineLayoutCreateInfo);

	// creating the descriptor pool

	vk::DescriptorPoolSize descriptorPoolSizes[] = {
		{vk::DescriptorType::eUniformBuffer, 1000},
		{vk::DescriptorType::eCombinedImageSampler, 1000}
	};

	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = {
		{},
		5,
		2,
		descriptorPoolSizes
	};

	std::tie(vkResult, m_DescriptorPool) = m_Device.createDescriptorPool(descriptorPoolCreateInfo);

	vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo = {
		m_DescriptorPool,
		1,
		&m_DescriptorSetLayout
	};

	std::vector<vk::DescriptorSet> descriptorSets;
	std::tie(vkResult, descriptorSets) = m_Device.allocateDescriptorSets(descriptorSetAllocateInfo);
	m_DescriptorSet = descriptorSets[0];

	vk::DescriptorBufferInfo descriptorBufferInfo = {
		m_UniformBuffer,
		0,
		sizeof(UniBuffer)
	};

	vk::WriteDescriptorSet descriptorWrite = {
		m_DescriptorSet,
		0,
		0,
		1,
		vk::DescriptorType::eUniformBuffer,
		nullptr,
		&descriptorBufferInfo
	};

	m_Device.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);

	// S4: shader stages
	vk::PipelineShaderStageCreateInfo vertShaderStageInfo = {
		{},
		vk::ShaderStageFlagBits::eVertex,
		m_TriangleVS,
		"main"
	};

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo = {
	{},
		vk::ShaderStageFlagBits::eFragment,
		m_TriangleFS,
		"main"
	};

	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// finally creating the pipeline
	vk::GraphicsPipelineCreateInfo pipelineCreateInfo = {
		{},
		2,
		shaderStages,
		&vertexInputStateCreateInfo,
		&inputAssemblyStateCreateInfo,
		nullptr,
		&viewportStateCreateInfo,
		&rasterizationStateCreateInfo,
		&multisampleStateCreateInfo,
		nullptr,
		&colorBlendStateCreateInfo,
		nullptr,
		m_PipelineLayout,
		m_RenderPass1,
		0,
		nullptr,
		-1
	};

	std::tie(vkResult, m_Pipeline) = m_Device.createGraphicsPipeline(nullptr, pipelineCreateInfo);

	if (vkResult != vk::Result::eSuccess)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "Unable to create graphics pipeline!", gEngine->GetViewport()->GetWindow());
		return EEngineStatus::Failed;
	}

	// creating the vertex buffer

	vk::BufferCreateInfo vertexBufferCreateInfo = {
		{},
		sizeof(kVertexData),
		vk::BufferUsageFlagBits::eVertexBuffer,
		vk::SharingMode::eExclusive
	};

	std::tie(vkResult, m_VertexBuffer) = m_Device.createBuffer(vertexBufferCreateInfo);
	VKR(vkResult);

	// allocate and bind memory for the buffer

	memoryRequirements = m_Device.getBufferMemoryRequirements(m_VertexBuffer);

	vk::MemoryAllocateInfo vertexBufferAllocInfo = {
		memoryRequirements.size,
		FindMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
	};

	std::tie(vkResult, m_VertexBufferMemory) = m_Device.allocateMemory(vertexBufferAllocInfo);
	VKR(vkResult);
	vkResult = m_Device.bindBufferMemory(m_VertexBuffer, m_VertexBufferMemory, 0);
	VKR(vkResult);

	// copy the vertex data
	void* pVertexDataDest;
	vkResult = m_Device.mapMemory(m_VertexBufferMemory, 0, sizeof(kVertexData), {}, &pVertexDataDest);
	VKR(vkResult);
	memcpy(pVertexDataDest, kVertexData, sizeof(kVertexData));
	m_Device.unmapMemory(m_VertexBufferMemory);

	// creating the index buffer

	vk::BufferCreateInfo indexBufferCreateInfo = {
		{},
		sizeof(kIndexData),
		vk::BufferUsageFlagBits::eIndexBuffer,
		vk::SharingMode::eExclusive
	};

	std::tie(vkResult, m_IndexBuffer) = m_Device.createBuffer(indexBufferCreateInfo);
	VKR(vkResult);

	memoryRequirements = m_Device.getBufferMemoryRequirements(m_IndexBuffer);

	vk::MemoryAllocateInfo indexBufferAllocInfo = {
		memoryRequirements.size,
		FindMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
	};

	std::tie(vkResult, m_IndexBufferMemory) = m_Device.allocateMemory(indexBufferAllocInfo);
	VKR(vkResult);
	vkResult = m_Device.bindBufferMemory(m_IndexBuffer, m_IndexBufferMemory, 0);
	VKR(vkResult);

	// copy the index data
	void* pIndexDataDest;
	vkResult = m_Device.mapMemory(m_IndexBufferMemory, 0, sizeof(kIndexData), {}, &pIndexDataDest);
	VKR(vkResult);
	memcpy(pIndexDataDest, kIndexData, sizeof(kIndexData));
	m_Device.unmapMemory(m_IndexBufferMemory);

	// creating the semaphores
	vk::SemaphoreCreateInfo semaphoreCreateInfo;
	std::tie(vkResult, m_ImageAvailableSemaphore) = m_Device.createSemaphore(semaphoreCreateInfo);
	VKR(vkResult);
	std::tie(vkResult, m_RenderFinished1Semaphore) = m_Device.createSemaphore(semaphoreCreateInfo);
	VKR(vkResult);
	std::tie(vkResult, m_RenderFinished2Semaphore) = m_Device.createSemaphore(semaphoreCreateInfo);
	VKR(vkResult);

	// creating the command pool
	vk::CommandPoolCreateInfo poolCreateInfo = {
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		selGraphicsFamily
	};
	std::tie(vkResult, m_CommandPool) = m_Device.createCommandPool(poolCreateInfo);
	VKR(vkResult);

	// creating the command buffers
	vk::CommandBufferAllocateInfo allocateInfo = {
		m_CommandPool,
		vk::CommandBufferLevel::ePrimary,
		static_cast<uint32_t>(m_SwapChainImageViews.size())
	};

	std::tie(vkResult, m_CommandBuffers) = m_Device.allocateCommandBuffers(allocateInfo);
	VKR(vkResult);

	// recording the command buffers
	i = 0;
	for (vk::CommandBuffer& commandBuffer : m_CommandBuffers)
	{
		vk::CommandBufferBeginInfo cbBeginInfo = {};

		vkResult = commandBuffer.begin(cbBeginInfo);
		VKR(vkResult);

		vk::ClearColorValue clearColor(std::array<float, 4>{0, 0, 0, 1.f});
		vk::ClearValue clearValue(clearColor);

		vk::RenderPassBeginInfo beginInfo = {
			m_RenderPass1,
			m_SwapChainFrameBuffers[i],
			{
				{0, 0},
				m_SwapChainExtent
			},
			1,
			&clearValue
		};

		commandBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Pipeline);

		vk::Buffer vertexBuffers[] = { m_VertexBuffer };
		vk::DeviceSize offsets[] = { 0 };
		commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

		commandBuffer.bindIndexBuffer(m_IndexBuffer, 0, vk::IndexType::eUint32);

		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

		commandBuffer.drawIndexed(3, 1, 0, 0, 0);

		commandBuffer.endRenderPass();

		vkResult = commandBuffer.end();
		VKR(vkResult);

		i++;
	}

	// >>> ImGui

	ImGui_ImplVulkan_InitInfo implVulkanInitInfo;
	memset(&implVulkanInitInfo, 0, sizeof(implVulkanInitInfo));
	implVulkanInitInfo.Instance = m_Instance;
	implVulkanInitInfo.PhysicalDevice = selPhysicalDevice;
	implVulkanInitInfo.Device = m_Device;
	implVulkanInitInfo.QueueFamily = selGraphicsFamily;
	implVulkanInitInfo.Queue = m_GraphicsQueue;
	implVulkanInitInfo.PipelineCache = nullptr;
	implVulkanInitInfo.DescriptorPool = m_DescriptorPool;
	implVulkanInitInfo.MinImageCount = 3;
	implVulkanInitInfo.ImageCount = 3;
	implVulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	implVulkanInitInfo.Allocator = nullptr;

	ImGui_ImplVulkan_Init(&implVulkanInitInfo, m_RenderPassImGui);

	const vk::CommandBufferAllocateInfo imGuiCbInfo = {
		m_CommandPool,
		vk::CommandBufferLevel::ePrimary,
		static_cast<uint32_t>(m_SwapChainImageViews.size())
	};

	std::tie(vkResult, m_CommandBuffersImGui) = m_Device.allocateCommandBuffers(imGuiCbInfo);

	// <<<

	// >>> ImGui Fonts

	{
		vk::CommandBuffer cb = m_CommandBuffersImGui[0];
		vkResult = cb.reset(vk::CommandBufferResetFlagBits::eReleaseResources);

		vk::CommandBufferBeginInfo cbBeginInfo = {
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit
		};
		vkResult = cb.begin(cbBeginInfo);

		ImGui_ImplVulkan_CreateFontsTexture(cb);

		vkResult = cb.end();

		vk::SubmitInfo cbSubmitInfo = {
			0,
			nullptr,
			nullptr,
			1,
			&cb,
			0,
			nullptr
		};

		vkResult = m_GraphicsQueue.submit(1, &cbSubmitInfo, nullptr);

		vkResult = m_Device.waitIdle();
	}

	// <<<

	return EEngineStatus::Ok;
}

inline float Lerp(const float a, const float b, const float f)
{
	return a + f * (b - a);
}

EEngineStatus CRender::Update(const float deltaTime)
{
	m_RotationSpeed = ClampValue(m_RotationSpeed, 0.f, 100000.f);
	m_ActualRotationSpeed = Lerp(m_ActualRotationSpeed, m_RotationSpeed, 0.0005f);
	m_ActualRotationSpeed = ClampValue(m_ActualRotationSpeed, 0.f, 100000.f);
	if (m_Angles >= 360.f)
	{
		m_Angles = 0.f;
	}

	m_Angles += m_ActualRotationSpeed * deltaTime;

	vk::Result vkResult;
	uint32_t imageIndex;

	std::tie(vkResult, imageIndex) = m_Device.acquireNextImageKHR(m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphore, nullptr, m_DispatchLoader);

	// updating the uniform buffer
	UniBuffer bufObj = {};
	bufObj.Angles = m_Angles; // TODO change to actual time
	bufObj.RotationSpeed = m_ActualRotationSpeed;

	void* uniformBufferMemory;
	m_Device.mapMemory(m_UniformBufferMemory, 0, sizeof(UniBuffer), {}, &uniformBufferMemory);
	memcpy(uniformBufferMemory, &bufObj, sizeof(UniBuffer));
	m_Device.unmapMemory(m_UniformBufferMemory);

	// submitting the command buffer
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	vk::SubmitInfo submitInfo = {
		1,
		&m_ImageAvailableSemaphore,
		waitStages,
		1,
		&m_CommandBuffers[imageIndex],
		1,
		&m_RenderFinished1Semaphore
	};

	vkResult = m_GraphicsQueue.submit(1, &submitInfo, nullptr);
	VKR(vkResult);

	// >>> ImGui
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame(gEngine->GetViewport()->GetWindow());
	ImGui::NewFrame();

	gEngine->OnRenderGui();
	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();

	const vk::CommandBufferBeginInfo cbBeginInfo = {};
	vkResult = m_CommandBuffersImGui[imageIndex].reset(vk::CommandBufferResetFlagBits::eReleaseResources);
	vkResult = m_CommandBuffersImGui[imageIndex].begin(cbBeginInfo);

	vk::RenderPassBeginInfo rpBeginInfo = {
		m_RenderPassImGui,
		m_SwapChainFrameBuffers[imageIndex],
		{
			{0, 0},
			m_SwapChainExtent
		},
		0,
		nullptr
	};

	m_CommandBuffersImGui[imageIndex].beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);
	ImGui_ImplVulkan_RenderDrawData(drawData, m_CommandBuffersImGui[imageIndex]);
	m_CommandBuffersImGui[imageIndex].endRenderPass();

	vkResult = m_CommandBuffersImGui[imageIndex].end();

	vk::SubmitInfo submitInfo2 = {
		1,
		&m_RenderFinished1Semaphore,
		waitStages,
		1,
		&m_CommandBuffersImGui[imageIndex],
		1,
		&m_RenderFinished2Semaphore
	};

	vkResult = m_GraphicsQueue.submit(1, &submitInfo2, nullptr);

	// <<<

	// presenting the image

	const vk::Semaphore presentWaitSemaphores[] = { m_RenderFinished1Semaphore, m_RenderFinished2Semaphore };

	const vk::PresentInfoKHR presentInfo = {
		2,
		presentWaitSemaphores,
		1,
		&m_SwapChain,
		&imageIndex
	};

	vkResult = m_GraphicsQueue.presentKHR(presentInfo, m_DispatchLoader);
	VKR(vkResult);

	vkResult = m_Device.waitIdle();
	VKR(vkResult)
		return EEngineStatus::Ok;
}

EEngineStatus CRender::Shutdown()
{
	SDL_Log("[CRender] Shutting down...");
	ImGui_ImplVulkan_Shutdown();
	m_Device.destroyDescriptorSetLayout(m_DescriptorSetLayout);
	m_Device.destroyDescriptorPool(m_DescriptorPool);
	m_Device.freeMemory(m_UniformBufferMemory);
	m_Device.destroyBuffer(m_UniformBuffer);
	m_Device.freeMemory(m_IndexBufferMemory);
	m_Device.destroyBuffer(m_IndexBuffer);
	m_Device.freeMemory(m_VertexBufferMemory);
	m_Device.destroyBuffer(m_VertexBuffer);
	m_Device.destroyPipeline(m_Pipeline);
	m_Device.destroyPipelineLayout(m_PipelineLayout);
	m_Device.freeCommandBuffers(m_CommandPool, m_CommandBuffers.size(), m_CommandBuffers.data());
	m_Device.destroyCommandPool(m_CommandPool);
	m_Device.destroySemaphore(m_ImageAvailableSemaphore);
	m_Device.destroySemaphore(m_RenderFinished1Semaphore);
	m_Device.destroySemaphore(m_RenderFinished2Semaphore);
	m_Device.destroyShaderModule(m_TriangleVS);
	m_Device.destroyShaderModule(m_TriangleFS);
	for (vk::Framebuffer& frameBuffer : m_SwapChainFrameBuffers)
	{
		m_Device.destroyFramebuffer(frameBuffer);
	}
	m_Device.destroyRenderPass(m_RenderPass1);
	m_Device.destroyRenderPass(m_RenderPassImGui);
	for (vk::ImageView& view : m_SwapChainImageViews)
	{
		m_Device.destroyImageView(view);
	}
	m_Device.destroySwapchainKHR(m_SwapChain, nullptr, m_DispatchLoader);
	m_Instance.destroySurfaceKHR(m_Surface, nullptr, m_DispatchLoader);
	m_Device.destroy();
#ifdef _DEBUG
	m_Instance.destroyDebugReportCallbackEXT(m_DebugReportCallback, nullptr, m_DispatchLoader);
#endif
	m_Instance.destroy();
	return EEngineStatus::Ok;
}

const char* CRender::GetGpuName() const
{
	return m_GpuName.c_str();
}

EEngineStatus CRender::LoadShadersTriangle()
{
	vk::Result vkResult;
#ifndef _DEBUG
	const char* const pathVS = "triangle.vert.spv";
	const char* const pathFS = "triangle.frag.spv";
#else
	const char* const pathVS = "../../shaders/triangle.vert.spv";
	const char* const pathFS = "../../shaders/triangle.frag.spv";
#endif

	std::ifstream fVS(pathVS, std::ios::ate | std::ios::binary);
	if (!fVS.is_open())
	{
		return EEngineStatus::Failed;
	}

	const size_t fVSSize = fVS.tellg();
	std::vector<char> vsBuffer(fVSSize);
	fVS.seekg(0);
	fVS.read(vsBuffer.data(), fVSSize);
	fVS.close();

	vk::ShaderModuleCreateInfo shaderModuleCreateInfo = {
		{},
		vsBuffer.size(),
		reinterpret_cast<const uint32_t*>(vsBuffer.data())
	};

	std::tie(vkResult, m_TriangleVS) = m_Device.createShaderModule(shaderModuleCreateInfo);
	if (vkResult != vk::Result::eSuccess)
	{
		return EEngineStatus::Failed;
	}

	std::ifstream fFS(pathFS, std::ios::ate | std::ios::binary);
	if (!fFS.is_open())
	{
		return EEngineStatus::Failed;
	}

	const size_t fFSSize = fFS.tellg();
	std::vector<char> fsBuffer(fFSSize);
	fFS.seekg(0);
	fFS.read(fsBuffer.data(), fFSSize);
	fFS.close();

	shaderModuleCreateInfo.codeSize = fsBuffer.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fsBuffer.data());

	std::tie(vkResult, m_TriangleFS) = m_Device.createShaderModule(shaderModuleCreateInfo);
	if (vkResult != vk::Result::eSuccess)
	{
		return EEngineStatus::Failed;
	}

	return EEngineStatus::Ok;
}

uint32_t CRender::FindMemoryType(const uint32_t typeFilter, const vk::MemoryPropertyFlags properties) const
{
	const vk::PhysicalDeviceMemoryProperties memoryProperties = m_PhysicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CRender Error", "Unable to find a !", gEngine->GetViewport()->GetWindow());
	std::abort();
}

