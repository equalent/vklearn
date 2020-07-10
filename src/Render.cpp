#include "Render.h"

#include <gsl/gsl_util>

#include "Viewport.h"
#include "SDL_vulkan.h"

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
#else
	const size_t additionalExtensionCount = 0;
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
		0,
		nullptr,
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

	auto inst = vkGetInstanceProcAddr(m_Instance, "vkCreateDebugReportCallbackEXT");

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
		uint32_t nPresent = 0;

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

	// creating the first render pass
	vk::AttachmentDescription colorAttachment = {
		{},
		m_SwapChainFormat,
		vk::SampleCountFlagBits::e1,
		vk::AttachmentLoadOp::eDontCare,
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
		1,
		&colorAttachmentRef
	};

	vk::RenderPassCreateInfo renderPassCreateInfo = {
		{},
		1,
		&colorAttachment,
		1,
		&subPassDesc
	};

	std::tie(vkResult, m_RenderPass1) = m_Device.createRenderPass(renderPassCreateInfo);
	VKR(vkResult);
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

	// creating the semaphores
	vk::SemaphoreCreateInfo semaphoreCreateInfo;
	std::tie(vkResult, m_ImageAvailableSemaphore) = m_Device.createSemaphore(semaphoreCreateInfo);
	VKR(vkResult);
	std::tie(vkResult, m_RenderFinishedSemaphore) = m_Device.createSemaphore(semaphoreCreateInfo);
	VKR(vkResult);

	// creating the command pool
	vk::CommandPoolCreateInfo poolCreateInfo = {
		{},
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

		vk::RenderPassBeginInfo beginInfo = {
			m_RenderPass1,
			m_SwapChainFrameBuffers[i],
			{
				{0, 0},
				m_SwapChainExtent
			},
			0
		};

		//commandBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

		vk::ClearColorValue clearColor(std::array<float, 4>{1.f, 0.f, 1.f, 1.f});
		vk::ImageSubresourceRange clearRange = {
			vk::ImageAspectFlagBits::eColor,
			0,
			1,
			0,
			1
		};
		commandBuffer.clearColorImage(swapChainImages[i], vk::ImageLayout::ePresentSrcKHR, clearColor, clearRange);

		//commandBuffer.endRenderPass();

		vkResult = commandBuffer.end();
		VKR(vkResult);

		i++;
	}

	return EEngineStatus::Ok;
}

EEngineStatus CRender::Update()
{
	vk::Result vkResult;
	uint32_t imageIndex;

	std::tie(vkResult, imageIndex) = m_Device.acquireNextImageKHR(m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphore, nullptr, m_DispatchLoader);



	// submitting the command buffer
	vk::Semaphore waitSemaphores[] = { m_ImageAvailableSemaphore };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	vk::Semaphore signalSemaphores[] = { m_RenderFinishedSemaphore };

	vk::SubmitInfo submitInfo = {
		1,
		waitSemaphores,
		waitStages,
		1,
		&m_CommandBuffers[imageIndex],
		1,
		signalSemaphores
	};

	vkResult = m_GraphicsQueue.submit(1, &submitInfo, nullptr);
	VKR(vkResult);

	// presenting the image

	const vk::PresentInfoKHR presentInfo = {
		1,
		&m_RenderFinishedSemaphore,
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
	m_Device.destroySemaphore(m_ImageAvailableSemaphore);
	for (vk::Framebuffer& frameBuffer : m_SwapChainFrameBuffers)
	{
		m_Device.destroyFramebuffer(frameBuffer);
	}
	m_Device.destroyRenderPass(m_RenderPass1);
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

