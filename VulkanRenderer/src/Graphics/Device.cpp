#include "Graphics/Device.hpp"

#include "Core/Window.hpp"
#include "Utils/Timer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>

Device::Device(DeviceCreateInfo& createInfo)
    : m_Layers(createInfo.layers), m_Extensions(createInfo.instanceExtensions), m_Window(createInfo.window)
{
	/////////////////////////////////////////////////////////////////////////////////
	// Initialize volk
	VKC(volkInitialize());

	/////////////////////////////////////////////////////////////////////////////////
	// Check if the required layers exist.
	{
		// Fetch supported layers
		uint32_t layersCount;
		vkEnumerateInstanceLayerProperties(&layersCount, nullptr);
		ASSERT(layersCount, "No instance layer found");

		std::vector<VkLayerProperties> availableLayers(layersCount);
		vkEnumerateInstanceLayerProperties(&layersCount, availableLayers.data());

		// Check if we support all the required layers
		for (const char* requiredLayerName : m_Layers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(requiredLayerName, layerProperties.layerName))
				{
					layerFound = true;
					break;
				}
			}

			ASSERT(layerFound, "Required layer not found");
		}
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create vulkan instance, window surface, debug messenger, and load the instace with volk.
	{
		VkApplicationInfo applicationInfo {
			.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName   = "Vulkan Renderer",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName        = "None",
			.engineVersion      = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion         = VK_API_VERSION_1_2,
		};

		VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {
			.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = createInfo.debugMessageSeverity,
			.messageType     = createInfo.debugMessageTypes,
			.pUserData       = nullptr,
		};

		// Setup validation layer message callback
		debugMessengerCreateInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		                                              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		                                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		                                              void* pUserData) {
			LOGVk(trace, "{}", pCallbackData->pMessage); // #TODO: Format this!
			return static_cast<VkBool32>(VK_FALSE);
		};

		// If debugging is not enabled, remove validation layer from instance layers
		if (!createInfo.enableDebugging)
		{
			auto validationLayerIt = std::find(createInfo.layers.begin(), createInfo.layers.end(), "VK_LAYER_KHRONOS_validation");
			if (validationLayerIt != createInfo.layers.end())
			{
				createInfo.layers.erase(validationLayerIt);
			}
		}

		// Create the vulkan instance
		VkInstanceCreateInfo instanceCreateInfo {
			.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext                   = createInfo.enableDebugging ? &debugMessengerCreateInfo : nullptr,
			.pApplicationInfo        = &applicationInfo,
			.enabledLayerCount       = static_cast<uint32_t>(m_Layers.size()),
			.ppEnabledLayerNames     = m_Layers.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(m_Extensions.size()),
			.ppEnabledExtensionNames = m_Extensions.data(),
		};

		VKC(vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance));
		volkLoadInstance(m_Instance);
		m_Surface = m_Window->CreateSurface(m_Instance);
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Iterate through physical devices that supports vulkan, and pick the most suitable one
	{
		// Fetch physical devices with vulkan support
		uint32_t deviceCount;
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
		ASSERT(deviceCount, "No physical device with vulkan support found");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

		// Select the most suitable physical device
		uint32_t highScore = 0u;
		for (const auto& device : devices)
		{
			uint32_t score = 0u;

			// Fetch properties & features
			VkPhysicalDeviceProperties properties;
			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceProperties(device, &properties);
			vkGetPhysicalDeviceFeatures(device, &features);

			if (!features.geometryShader)
				continue;

			/** Check if the device supports the required queues **/
			{
				// Fetch queue families
				uint32_t queueFamiliesCount;
				vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamiliesCount, nullptr);
				if (!queueFamiliesCount)
					continue;

				std::vector<VkQueueFamilyProperties> queueFamiliesProperties(queueFamiliesCount);
				vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamiliesCount, queueFamiliesProperties.data());

				uint32_t index = 0u;
				for (const auto& queueFamilyProperties : queueFamiliesProperties)
				{
					// Check if current queue index supports a desired queue
					if (queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
					{
						m_GraphicsQueueIndex = index;
					}

					VkBool32 presentSupport;
					vkGetPhysicalDeviceSurfaceSupportKHR(device, index, m_Surface, &presentSupport);
					if (presentSupport)
					{
						m_PresentQueueIndex = index;
					}

					index++;

					// Device does supports all the required queues!
					if (m_GraphicsQueueIndex != UINT32_MAX && m_PresentQueueIndex != UINT32_MAX)
					{
						break;
					}
				}

				// Device doesn't support all the required queues!
				if (m_GraphicsQueueIndex == UINT32_MAX || m_PresentQueueIndex == UINT32_MAX)
				{
					m_GraphicsQueueIndex = UINT32_MAX;
					m_PresentQueueIndex  = UINT32_MAX;
					continue;
				}
			}

			/** Check if device supports required extensions **/
			{
				// Fetch extensions
				uint32_t extensionsCount;
				vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, nullptr);
				if (!extensionsCount)
					continue;

				std::vector<VkExtensionProperties> deviceExtensionsProperties(extensionsCount);
				vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, deviceExtensionsProperties.data());

				bool failed = false;
				for (const auto& requiredExtensionName : createInfo.logicalDeviceExtensions)
				{
					bool found = false;
					for (const auto& extension : deviceExtensionsProperties)
					{
						if (!strcmp(requiredExtensionName, extension.extensionName))
						{
							found = true;
							break;
						}
					}

					if (!found)
					{
						failed = true;
						break;
					}
				}

				if (failed)
					continue;
			}

			/** Check if swap chain is adequate **/
			{
				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &m_SurfcaCapabilities);

				// Fetch swap chain formats
				uint32_t formatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
				if (!formatCount)
					continue;

				m_SupportedSurfaceFormats.clear();
				m_SupportedSurfaceFormats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, m_SupportedSurfaceFormats.data());

				// Fetch swap chain present modes
				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);
				if (!presentModeCount)
					continue;

				m_SupportedPresentModes.clear();
				m_SupportedPresentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, m_SupportedPresentModes.data());
			}

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				score += 69420; // nice

			score += properties.limits.maxImageDimension2D;

			m_PhysicalDevice = score > highScore ? device : m_PhysicalDevice;
		}
		ASSERT(m_PhysicalDevice, "No suitable physical device found");

		// Store the selected physical device's properties
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_PhysicalDeviceProperties);
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create the logical device.
	{
		std::vector<VkDeviceQueueCreateInfo> queuesCreateInfo;

		float queuePriority = 1.0f; // Always 1.0
		queuesCreateInfo.push_back({
		    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		    .queueFamilyIndex = m_GraphicsQueueIndex,
		    .queueCount       = 1u,
		    .pQueuePriorities = &queuePriority,
		});

		if (m_PresentQueueIndex != m_GraphicsQueueIndex)
		{
			queuesCreateInfo.push_back({
			    .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			    .queueFamilyIndex = m_PresentQueueIndex,
			    .queueCount       = 1u,
			    .pQueuePriorities = &queuePriority,
			});
		}

		// No features needed ATM
		VkPhysicalDeviceFeatures physicalDeviceFeatures {}; // #TODO

		VkDeviceCreateInfo logicalDeviceCreateInfo {
			.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount    = static_cast<uint32_t>(queuesCreateInfo.size()),
			.pQueueCreateInfos       = queuesCreateInfo.data(),
			.enabledExtensionCount   = static_cast<uint32_t>(createInfo.logicalDeviceExtensions.size()),
			.ppEnabledExtensionNames = createInfo.logicalDeviceExtensions.data(),
			.pEnabledFeatures        = &physicalDeviceFeatures,
		};

		VKC(vkCreateDevice(m_PhysicalDevice, &logicalDeviceCreateInfo, nullptr, &m_LogicalDevice));

		vkGetDeviceQueue(m_LogicalDevice, m_GraphicsQueueIndex, 0u, &m_GraphicsQueue);
		vkGetDeviceQueue(m_LogicalDevice, m_PresentQueueIndex, 0u, &m_PresentQueue);

		ASSERT(m_GraphicsQueue, "Failed to fetch graphics queue");
		ASSERT(m_PresentQueue, "Failed to fetch present queue");
	}


	/////////////////////////////////////////////////////////////////////////////////
	// Create sync objects
	{
		m_AquireImageSemaphores.resize(m_MaxFramesInFlight);
		m_RenderSemaphores.resize(m_MaxFramesInFlight);
		m_FrameFences.resize(m_MaxFramesInFlight);

		for (uint32_t i = 0; i < m_MaxFramesInFlight; i++)
		{
			VkSemaphoreCreateInfo semaphoreCreateInfo {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			};

			VkFenceCreateInfo fenceCreateInfo {
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT,
			};

			VKC(vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, nullptr, &m_AquireImageSemaphores[i]));
			VKC(vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, nullptr, &m_RenderSemaphores[i]));
			VKC(vkCreateFence(m_LogicalDevice, &fenceCreateInfo, nullptr, &m_FrameFences[i]));
		}
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create the swapchain & swapchain dependent objects(swapchain, images, image views, renderpass, framebuffers, commandpool, pipelines)
	CreateSwapchain();

	/////////////////////////////////////////////////////////////////////////////////
	// Log debug information about the selected physical device, layers, extensions, etc.
	{
		LOG(info, "Device created:");

		LOG(info, "    PhysicalDevice:");
		LOG(info, "        apiVersion: {}", m_PhysicalDeviceProperties.apiVersion);
		LOG(info, "        driverVersion: {}", m_PhysicalDeviceProperties.driverVersion);
		LOG(info, "        vendorID: {}", m_PhysicalDeviceProperties.vendorID);
		LOG(info, "        deviceID: {}", m_PhysicalDeviceProperties.deviceID);
		LOG(info, "        deviceType: {}", m_PhysicalDeviceProperties.deviceType); // #todo: Stringify
		LOG(info, "        deviceName: {}", m_PhysicalDeviceProperties.deviceName);

		LOG(info, "    Layers:");
		for (auto layer : m_Layers)
			LOG(info, "        {}", layer);

		LOG(info, "    Extensions:");
		for (auto extension : m_Extensions)
			LOG(info, "        {}", extension);

		LOG(info, "    Queues:");
		LOG(info, "        Graphics: {}", m_GraphicsQueueIndex);
		LOG(info, "        Present: {}", m_PresentQueueIndex);

		LOG(info, "    Swapchain:");
		LOG(info, "        imageCount: {}", m_Images.size());
		LOG(info, "        extent: {}x{}", m_SwapchainExtent.width, m_SwapchainExtent.height);
	}
}

Device::~Device()
{
	vkDeviceWaitIdle(m_LogicalDevice);

	for (uint32_t i = 0; i < m_MaxFramesInFlight; i++)
	{
		vkDestroySemaphore(m_LogicalDevice, m_AquireImageSemaphores[i], nullptr);
		vkDestroySemaphore(m_LogicalDevice, m_RenderSemaphores[i], nullptr);
		vkDestroyFence(m_LogicalDevice, m_FrameFences[i], nullptr);
	}

	DestroySwapchain();

	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_DescriptorSetLayout, nullptr);

	vkDestroyDevice(m_LogicalDevice, nullptr);

	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	vkDestroyInstance(m_Instance, nullptr);
}

void Device::DrawFrame()
{
	// Timer...
	static Timer timer;
	float time = timer.ElapsedTime();

	// Wait for the frame fence
	VKC(vkWaitForFences(m_LogicalDevice, 1u, &m_FrameFences[m_CurrentFrame], VK_TRUE, UINT64_MAX));

	// Acquire an image
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(m_LogicalDevice, m_Swapchain, UINT64_MAX, m_AquireImageSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		VKC(vkDeviceWaitIdle(m_LogicalDevice));
		DestroySwapchain();
		CreateSwapchain();
		return;
	}
	else
	{
		ASSERT(result == VK_SUCCESS, "VkAcquireNextImage failed without returning VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR");
	}

	// Update model view projection uniform
	UniformMVP mvp;
	mvp.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	mvp.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	mvp.proj  = glm::perspective(glm::radians(45.0f), m_SwapchainExtent.width / (float)m_SwapchainExtent.height, 0.1f, 10.0f);

	void* mvpMap = m_MVPUniBuffer[m_CurrentFrame]->Map();
	memcpy(mvpMap, &mvp, sizeof(UniformMVP));
	m_MVPUniBuffer[m_CurrentFrame]->Unmap();

	// Record commands
	CommandBufferStartInfo commandBufferStartInfo {
		.mvpDescriptorSet = &m_MVPDescriptorSet[m_CurrentFrame],
		.framebuffer      = m_Framebuffers[imageIndex],
		.extent           = m_SwapchainExtent,
		.frameIndex       = m_CurrentFrame,
	};
	VkCommandBuffer firstTriangleCommandBuffer = m_TrianglePipeline->RecordCommandBuffer(commandBufferStartInfo);

	// Submit commands (and reset the fence)
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo {
		.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount   = 1u,
		.pWaitSemaphores      = &m_AquireImageSemaphores[m_CurrentFrame],
		.pWaitDstStageMask    = &waitStage,
		.commandBufferCount   = 1u,
		.pCommandBuffers      = &firstTriangleCommandBuffer,
		.signalSemaphoreCount = 1u,
		.pSignalSemaphores    = &m_RenderSemaphores[m_CurrentFrame],
	};

	VKC(vkResetFences(m_LogicalDevice, 1u, &m_FrameFences[m_CurrentFrame]));
	VKC(vkQueueSubmit(m_GraphicsQueue, 1u, &submitInfo, m_FrameFences[m_CurrentFrame]));

	// Present the image
	VkPresentInfoKHR presentInfo {
		.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1u,
		.pWaitSemaphores    = &m_RenderSemaphores[m_CurrentFrame],
		.swapchainCount     = 1u,
		.pSwapchains        = &m_Swapchain,
		.pImageIndices      = &imageIndex,
		.pResults           = nullptr
	};

	vkQueuePresentKHR(m_PresentQueue, &presentInfo);

	// Increment frame index
	m_CurrentFrame = (m_CurrentFrame + 1u) % m_MaxFramesInFlight;
}

void Device::CreateSwapchain()
{
	/////////////////////////////////////////////////////////////////////////////////
	// Fetch the swapchain details
	{
		// Fetch surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &m_SurfcaCapabilities);

		// Fetch swap chain formats
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
		m_SupportedSurfaceFormats.clear();
		m_SupportedSurfaceFormats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, m_SupportedSurfaceFormats.data());
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create the swap chain.
	{
		// Select surface format
		m_SurfaceFormat = m_SupportedSurfaceFormats[0]; // default
		for (const auto& surfaceFormat : m_SupportedSurfaceFormats)
		{
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				m_SurfaceFormat = surfaceFormat;
				break;
			}
		}

		// Select present mode
		m_PresentMode = m_SupportedPresentModes[0]; // default
		for (const auto& presentMode : m_SupportedPresentModes)
		{
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				m_PresentMode = presentMode;
			}
		}

		// Select extent
		m_SwapchainExtent = m_SurfcaCapabilities.currentExtent;
		if (m_SwapchainExtent.width == UINT32_MAX)
		{
			VkExtent2D framebufferSize = m_Window->GetFramebufferSize();
			framebufferSize.width      = std::clamp(framebufferSize.width, m_SurfcaCapabilities.minImageExtent.width, m_SurfcaCapabilities.maxImageExtent.width);
			framebufferSize.height     = std::clamp(framebufferSize.height, m_SurfcaCapabilities.minImageExtent.height, m_SurfcaCapabilities.maxImageExtent.height);
		}

		// Select image count ; one more than minImageCount, if minImageCount +1 is not higher than maxImageCount (maxImageCount of 0 means no limit)
		uint32_t imageCount = m_SurfcaCapabilities.maxImageCount > 0 && m_SurfcaCapabilities.minImageCount + 1 > m_SurfcaCapabilities.maxImageCount ? m_SurfcaCapabilities.maxImageCount : m_SurfcaCapabilities.minImageCount + 1;

		// Create swapchain
		bool sameQueueIndex = m_GraphicsQueueIndex == m_PresentQueueIndex;
		VkSwapchainCreateInfoKHR swapchainCreateInfo {
			.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface               = m_Surface,
			.minImageCount         = imageCount,
			.imageFormat           = m_SurfaceFormat.format,
			.imageColorSpace       = m_SurfaceFormat.colorSpace,
			.imageExtent           = m_SwapchainExtent,
			.imageArrayLayers      = 1u,
			.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // Write directly to the image (we would use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT if we wanted to do post-processing)
			.imageSharingMode      = sameQueueIndex ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
			.queueFamilyIndexCount = sameQueueIndex ? 0u : 2u,
			.pQueueFamilyIndices   = sameQueueIndex ? nullptr : &m_GraphicsQueueIndex,
			.preTransform          = m_SurfcaCapabilities.currentTransform,
			.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // No alpha-blending between multiple windows
			.presentMode           = m_PresentMode,
			.clipped               = VK_TRUE, // Don't render the obsecured pixels
			.oldSwapchain          = VK_NULL_HANDLE,
		};

		VKC(vkCreateSwapchainKHR(m_LogicalDevice, &swapchainCreateInfo, nullptr, &m_Swapchain));
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Fetch swap chain images and create image views
	{
		// Fetch images
		uint32_t imageCount;
		vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &imageCount, nullptr);
		m_Images.resize(imageCount);
		m_ImageViews.resize(imageCount);
		vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &imageCount, m_Images.data());

		for (uint32_t i = 0; i < m_Images.size(); i++)
		{
			VkImageViewCreateInfo imageViewCreateInfo {
				.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image      = m_Images[i],
				.viewType   = VK_IMAGE_VIEW_TYPE_2D,
				.format     = m_SurfaceFormat.format,
				.components = {
				    // Don't swizzle the colors around...
				    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
				    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
				    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
				    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
				},
				.subresourceRange = {
				    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, // Image will be used as color target
				    .baseMipLevel   = 0,                         // No mipmaipping
				    .levelCount     = 1,                         // No levels
				    .baseArrayLayer = 0,                         // No nothin...
				    .layerCount     = 1,
				},
			};

			VKC(vkCreateImageView(m_LogicalDevice, &imageViewCreateInfo, nullptr, &m_ImageViews[i]));
		}
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Specify the attachments and subpasses and create the renderpass
	{
		// Attachment
		VkAttachmentDescription colorAttachmentDesc {
			.format         = m_SurfaceFormat.format,
			.samples        = VK_SAMPLE_COUNT_1_BIT,
			.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};

		// Subpass
		VkAttachmentReference colorAttachmentRef {
			.attachment = 0u,
			.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpassDesc {
			.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1u,
			.pColorAttachments    = &colorAttachmentRef,
		};

		// Subpass dependency
		VkSubpassDependency subpassDependency {
			.srcSubpass    = VK_SUBPASS_EXTERNAL,
			.dstSubpass    = 0u,
			.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0u,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		};

		// Renderpass
		VkRenderPassCreateInfo renderPassCreateInfo {
			.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1u,
			.pAttachments    = &colorAttachmentDesc,
			.subpassCount    = 1u,
			.pSubpasses      = &subpassDesc,
			.dependencyCount = 1u,
			.pDependencies   = &subpassDependency,
		};

		VKC(vkCreateRenderPass(m_LogicalDevice, &renderPassCreateInfo, nullptr, &m_RenderPass));
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create the framebuffers
	{
		m_Framebuffers.resize(m_Images.size());
		for (uint32_t i = 0; i < m_Framebuffers.size(); i++)
		{
			VkFramebufferCreateInfo framebufferCreateInfo {
				.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass      = m_RenderPass,
				.attachmentCount = 1u,
				.pAttachments    = &m_ImageViews[i],
				.width           = m_SwapchainExtent.width,
				.height          = m_SwapchainExtent.height,
				.layers          = 1u,
			};

			VKC(vkCreateFramebuffer(m_LogicalDevice, &framebufferCreateInfo, nullptr, &m_Framebuffers[i]));
		}
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create the command pool
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = m_GraphicsQueueIndex,
		};

		VKC(vkCreateCommandPool(m_LogicalDevice, &commandPoolCreateInfo, nullptr, &m_CommandPool));
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create descriptor pool
	{
		VkDescriptorPoolSize descriptorPoolSize {
			.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = m_MaxFramesInFlight,
		};

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo {
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets       = m_MaxFramesInFlight,
			.poolSizeCount = 1u,
			.pPoolSizes    = &descriptorPoolSize,
		};

		VKC(vkCreateDescriptorPool(m_LogicalDevice, &descriptorPoolCreateInfo, nullptr, &m_DescriptorPool));
	}
	/////////////////////////////////////////////////////////////////////////////////
	// Specify descriptor set layout bindings and create a DescriptorSetLayout
	{
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

		// Model view projection
		layoutBindings.push_back(VkDescriptorSetLayoutBinding {
		    .binding            = 0u,
		    .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		    .descriptorCount    = 1u,
		    .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
		    .pImmutableSamplers = nullptr });

		// Create descriptor set layout
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
			.pBindings    = layoutBindings.data(),
		};
		VKC(vkCreateDescriptorSetLayout(m_LogicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &m_DescriptorSetLayout));
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create uniform buffers
	{
		BufferCreateInfo mvpBufferCreateInfo {
			.logicalDevice  = m_LogicalDevice,
			.physicalDevice = m_PhysicalDevice,
			.commandPool    = m_CommandPool,
			.graphicsQueue  = m_GraphicsQueue,
			.usage          = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.size           = sizeof(UniformMVP),
		};

		m_MVPUniBuffer.resize(m_MaxFramesInFlight);
		for (uint32_t i = 0; i < m_MaxFramesInFlight; i++)
		{
			m_MVPUniBuffer[i] = std::make_unique<Buffer>(mvpBufferCreateInfo);
		}
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create descriptor sets
	{
		m_MVPDescriptorSet.resize(m_MaxFramesInFlight);

		VkDescriptorSetAllocateInfo descriptorSetAllocInfo {
			.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool     = m_DescriptorPool,
			.descriptorSetCount = 1u,
			.pSetLayouts        = &m_DescriptorSetLayout,
		};

		for (uint32_t i = 0; i < m_MaxFramesInFlight; i++)
		{
			VKC(vkAllocateDescriptorSets(m_LogicalDevice, &descriptorSetAllocInfo, &m_MVPDescriptorSet[i]));
			VkDescriptorBufferInfo descriptorBufferInfo {
				.buffer = *m_MVPUniBuffer[i]->GetBuffer(),
				.offset = 0u,
				.range  = VK_WHOLE_SIZE, // sizeof(UniformMVP)
			};

		    VkWriteDescriptorSet writeDescriptorSet {
			    .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			    .dstSet           = m_MVPDescriptorSet[i],
			    .dstBinding       = 0u,
			    .dstArrayElement  = 0u,
			    .descriptorCount  = 1u,
			    .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			    .pImageInfo       = VK_NULL_HANDLE,
			    .pBufferInfo      = &descriptorBufferInfo,
			    .pTexelBufferView = VK_NULL_HANDLE,
		    };

			vkUpdateDescriptorSets(m_LogicalDevice, 1u, &writeDescriptorSet, 0u, nullptr);
		}
	}

	/////////////////////////////////////////////////////////////////////////////////
	// Create pipelines
	{
		PipelineCreateInfo pipelineCreateInfo {
			.logicalDevice  = m_LogicalDevice,
			.physicalDevice = m_PhysicalDevice,
			.graphicsQueue  = m_GraphicsQueue,
			.viewportExtent = m_SwapchainExtent,
			.commandPool    = m_CommandPool,
			.imageCount     = static_cast<uint32_t>(m_Images.size()),
			.renderPass     = m_RenderPass,

			// Shader
			.descriptorSetLayouts = { m_DescriptorSetLayout },
			.vertexShaderPath     = "VulkanRenderer/res/vertex.glsl",
			.pixelShaderPath      = "VulkanRenderer/res/pixel.glsl",

			// Vertex input
			.vertexBindingDesc = VkVertexInputBindingDescription {
			    .binding   = 0u,
			    .stride    = sizeof(glm::vec3) + sizeof(glm::vec3),
			    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			},
			.vertexAttribDescs = {
			    VkVertexInputAttributeDescription {
			        .location = 0u,
			        .binding  = 0u,
			        .format   = VK_FORMAT_R32G32B32_SFLOAT,
			        .offset   = 0u,
			    },
			    VkVertexInputAttributeDescription {
			        .location = 1u,
			        .binding  = 0u,
			        .format   = VK_FORMAT_R32G32B32_SFLOAT,
			        .offset   = sizeof(glm::vec3),
			    },
			},
		};
		m_TrianglePipeline = std::make_unique<Pipeline>(pipelineCreateInfo);
	}
}

void Device::DestroySwapchain()
{
	vkDestroySwapchainKHR(m_LogicalDevice, m_Swapchain, nullptr);
	for (uint32_t i = 0; i < m_Images.size(); i++)
	{
		vkDestroyImageView(m_LogicalDevice, m_ImageViews[i], nullptr);
		vkDestroyFramebuffer(m_LogicalDevice, m_Framebuffers[i], nullptr);
	}
	vkDestroyRenderPass(m_LogicalDevice, m_RenderPass, nullptr);
	vkDestroyCommandPool(m_LogicalDevice, m_CommandPool, nullptr);
	m_MVPUniBuffer.clear();
	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_DescriptorSetLayout, nullptr);
	m_TrianglePipeline.reset();
}
