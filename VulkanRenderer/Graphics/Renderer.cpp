#include "Graphics/Renderer.hpp"

#include "Core/Window.hpp"
#include "Graphics/Types.hpp"
#include "Scene/Scene.hpp"
#include "Utils/CVar.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <filesystem>
#include <imgui.h>

Renderer::Renderer(const Renderer::CreateInfo& info)
    : m_LogicalDevice(info.deviceContext.logicalDevice)
    , m_Allocator(info.deviceContext.allocator)
    , m_DepthFormat(info.deviceContext.depthFormat)
    , m_DefaultTexture(info.defaultTexture)
    , m_SkyboxTexture(info.skyboxTexture)

{
	// Validate create info
	ASSERT(info.window && info.skyboxTexture && info.defaultTexture,
	       "Incomplete renderer create info");

	CreateSyncObjects();
	CreateDescriptorPools();
	RecreateSwapchainResources(info.window, info.deviceContext);
}

Renderer::~Renderer()
{
	DestroySwapchain();

	m_LogicalDevice.destroyDescriptorPool(m_DescriptorPool, nullptr);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_LogicalDevice.destroyFence(m_Frames[i].renderFence, nullptr);
		m_LogicalDevice.destroySemaphore(m_Frames[i].renderSemaphore, nullptr);
		m_LogicalDevice.destroySemaphore(m_Frames[i].presentSemaphore, nullptr);
	}

	m_LogicalDevice.destroyFence(m_UploadContext.fence);
}

void Renderer::RecreateSwapchainResources(Window* window, DeviceContext deviceContext)
{
	DestroySwapchain();

	m_SurfaceInfo = deviceContext.surfaceInfo;
	m_QueueInfo   = deviceContext.queueInfo;
	m_SampleCount = deviceContext.maxSupportedSampleCount;

	CreateSwapchain();
	CreateImageViews();

	CreateResolveColorImage();
	CreateDepthImage();

	CreateCommandPool();

	CreateDescriptorResources(deviceContext.physicalDevice);
	CreateFramePass();
	CreateForwardPass(deviceContext);
	CreateUIPass(window, deviceContext);

	WriteDescriptorSetsDefaultValues();
}

void Renderer::CreateSyncObjects()
{
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::SemaphoreCreateInfo semaphoreCreateInfo {};

		vk::FenceCreateInfo fenceCreateInfo {
			vk::FenceCreateFlagBits::eSignaled, // flags
		};

		m_Frames[i].renderFence      = m_LogicalDevice.createFence(fenceCreateInfo, nullptr);
		m_Frames[i].renderSemaphore  = m_LogicalDevice.createSemaphore(semaphoreCreateInfo, nullptr);
		m_Frames[i].presentSemaphore = m_LogicalDevice.createSemaphore(semaphoreCreateInfo, nullptr);
	}

	m_UploadContext.fence = m_LogicalDevice.createFence({}, nullptr);
}

void Renderer::CreateDescriptorPools()
{
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		{ vk::DescriptorType::eSampler, 1000 },
		{ vk::DescriptorType::eCombinedImageSampler, 1000 },
		{ vk::DescriptorType::eSampledImage, 1000 },
		{ vk::DescriptorType::eStorageImage, 1000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		{ vk::DescriptorType::eUniformBuffer, 1000 },
		{ vk::DescriptorType::eStorageBuffer, 1000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		{ vk::DescriptorType::eInputAttachment, 1000 }
	};
	// descriptorPoolSizes.push_back(VkDescriptorPoolSize {});

	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {
		{},                                      // flags
		100,                                     // maxSets
		static_cast<uint32_t>(poolSizes.size()), // poolSizeCount
		poolSizes.data(),                        // pPoolSizes
	};

	m_DescriptorPool = m_LogicalDevice.createDescriptorPool(descriptorPoolCreateInfo, nullptr);
}

void Renderer::CreateSwapchain()
{
	const uint32_t minImage = m_SurfaceInfo.capabilities.minImageCount;
	const uint32_t maxImage = m_SurfaceInfo.capabilities.maxImageCount;

	uint32_t imageCount = maxImage >= DESIRED_SWAPCHAIN_IMAGES && minImage <= DESIRED_SWAPCHAIN_IMAGES ? DESIRED_SWAPCHAIN_IMAGES :
	                      maxImage == 0ul && minImage <= DESIRED_SWAPCHAIN_IMAGES                      ? DESIRED_SWAPCHAIN_IMAGES :
	                      minImage <= 2ul && maxImage >= 2ul                                           ? 2ul :
	                      minImage == 0ul && maxImage >= 2ul                                           ? 2ul :
	                                                                                                     minImage;
	// Create swapchain
	bool sameQueueIndex = m_QueueInfo.graphicsQueueIndex == m_QueueInfo.presentQueueIndex;
	vk::SwapchainCreateInfoKHR swapchainCreateInfo {
		{},                                                                          // flags
		m_SurfaceInfo.surface,                                                       // surface
		imageCount,                                                                  // minImageCount
		m_SurfaceInfo.format.format,                                                 // imageFormat
		m_SurfaceInfo.format.colorSpace,                                             // imageColorSpace
		m_SurfaceInfo.capabilities.currentExtent,                                    // imageExtent
		1u,                                                                          // imageArrayLayers
		vk::ImageUsageFlagBits::eColorAttachment,                                    // imageUsage -> Write directly to the image (use VK_IMAGE_USAGE_TRANSFER_DST_BIT for post-processing) ??
		sameQueueIndex ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent, // imageSharingMode
		sameQueueIndex ? 0u : 2u,                                                    // queueFamilyIndexCount
		sameQueueIndex ? nullptr : &m_QueueInfo.graphicsQueueIndex,                  // pQueueFamilyIndices
		m_SurfaceInfo.capabilities.currentTransform,                                 // preTransform
		vk::CompositeAlphaFlagBitsKHR::eOpaque,                                      // compositeAlpha -> No alpha-blending between multiple windows
		m_SurfaceInfo.presentMode,                                                   // presentMode
		VK_TRUE,                                                                     // clipped -> Don't render the obsecured pixels
		VK_NULL_HANDLE,                                                              // oldSwapchain
	};

	m_Swapchain = m_LogicalDevice.createSwapchainKHR(swapchainCreateInfo, nullptr);
	m_Images    = m_LogicalDevice.getSwapchainImagesKHR(m_Swapchain);
}

void Renderer::CreateImageViews()
{
	for (uint32_t i = 0; i < m_Images.size(); i++)
	{
		vk::ImageViewCreateInfo imageViewCreateInfo {
			{},                          // flags
			m_Images[i],                 // image
			vk::ImageViewType::e2D,      // viewType
			m_SurfaceInfo.format.format, // format

			/* components */
			vk::ComponentMapping {
			    // Don't swizzle the colors around...
			    vk::ComponentSwizzle::eIdentity, // r
			    vk::ComponentSwizzle::eIdentity, // g
			    vk::ComponentSwizzle::eIdentity, // b
			    vk::ComponentSwizzle::eIdentity, // a
			},

			/* subresourceRange */
			vk::ImageSubresourceRange {
			    vk::ImageAspectFlagBits::eColor, // Image will be used as color
			                                     // target // aspectMask
			    0,                               // No mipmaipping // baseMipLevel
			    1,                               // No levels // levelCount
			    0,                               // No nothin... // baseArrayLayer
			    1,                               // layerCount
			},
		};

		m_ImageViews.push_back(m_LogicalDevice.createImageView(imageViewCreateInfo, nullptr));
	}
}

void Renderer::CreateResolveColorImage()
{
	vk::ImageCreateInfo imageCreateInfo {
		{},                          // flags
		vk::ImageType::e2D,          // imageType
		m_SurfaceInfo.format.format, // format

		/* extent */
		vk::Extent3D {
		    m_SurfaceInfo.capabilities.currentExtent.width,  // width
		    m_SurfaceInfo.capabilities.currentExtent.height, // height
		    1u,                                              // depth
		},
		1u,                        // mipLevels
		1u,                        // arrayLayers
		m_SampleCount,             // samples
		vk::ImageTiling::eOptimal, // tiling
		vk::ImageUsageFlagBits::eTransientAttachment |
		    vk::ImageUsageFlagBits::eColorAttachment, // usage
		vk::SharingMode::eExclusive,                  // sharingMode
		0u,                                           // queueFamilyIndexCount
		nullptr,                                      // pQueueFamilyIndices
		vk::ImageLayout::eUndefined,                  // initialLayout
	};

	vma::AllocationCreateInfo imageAllocInfo(
	    {}, vma::MemoryUsage::eGpuOnly,
	    vk::MemoryPropertyFlagBits::eDeviceLocal);
	m_ColorImage = m_Allocator.createImage(imageCreateInfo, imageAllocInfo);

	// Create color image-view
	vk::ImageViewCreateInfo imageViewCreateInfo {
		{},                          // flags
		m_ColorImage,                // image
		vk::ImageViewType::e2D,      // viewType
		m_SurfaceInfo.format.format, // format

		/* components */
		vk::ComponentMapping {
		    // Don't swizzle the colors around...
		    vk::ComponentSwizzle::eIdentity, // r
		    vk::ComponentSwizzle::eIdentity, // g
		    vk::ComponentSwizzle::eIdentity, // b
		    vk::ComponentSwizzle::eIdentity, // a
		},

		/* subresourceRange */
		vk::ImageSubresourceRange {
		    vk::ImageAspectFlagBits::eColor, // aspectMask
		    0u,                              // baseMipLevel
		    1u,                              // levelCount
		    0u,                              // baseArrayLayer
		    1u,                              // layerCount
		},
	};

	m_ColorImageView = m_LogicalDevice.createImageView(imageViewCreateInfo, nullptr);
}

void Renderer::CreateDepthImage()
{
	// Create depth image
	vk::ImageCreateInfo imageCreateInfo {
		{},                 // flags
		vk::ImageType::e2D, // imageType
		m_DepthFormat,      // format

		/* extent */
		vk::Extent3D {
		    m_SurfaceInfo.capabilities.currentExtent.width,  // width
		    m_SurfaceInfo.capabilities.currentExtent.height, // height
		    1u,                                              // depth
		},
		1u,                                              // mipLevels
		1u,                                              // arrayLayers
		m_SampleCount,                                   // samples
		vk::ImageTiling::eOptimal,                       // tiling
		vk::ImageUsageFlagBits::eDepthStencilAttachment, // usage
		vk::SharingMode::eExclusive,                     // sharingMode
		0u,                                              // queueFamilyIndexCount
		nullptr,                                         // pQueueFamilyIndices
		vk::ImageLayout::eUndefined,                     // initialLayout
	};

	vma::AllocationCreateInfo imageAllocInfo({}, vma::MemoryUsage::eGpuOnly, vk::MemoryPropertyFlagBits::eDeviceLocal);

	m_DepthImage = m_Allocator.createImage(imageCreateInfo, imageAllocInfo);

	// Create depth image-view
	vk::ImageViewCreateInfo imageViewCreateInfo {
		{},                     // flags
		m_DepthImage,           // image
		vk::ImageViewType::e2D, // viewType
		m_DepthFormat,          // format

		/* components */
		vk::ComponentMapping {
		    // Don't swizzle the colors around...
		    vk::ComponentSwizzle::eIdentity, // t
		    vk::ComponentSwizzle::eIdentity, // g
		    vk::ComponentSwizzle::eIdentity, // b
		    vk::ComponentSwizzle::eIdentity, // a
		},

		/* subresourceRange */
		vk::ImageSubresourceRange {
		    vk::ImageAspectFlagBits::eDepth, // aspectMask
		    0u,                              // baseMipLevel
		    1u,                              // levelCount
		    0u,                              // baseArrayLayer
		    1u,                              // layerCount
		},
	};

	m_DepthImageView = m_LogicalDevice.createImageView(imageViewCreateInfo, nullptr);
}

void Renderer::CreateCommandPool()
{
	// renderer
	vk::CommandPoolCreateInfo commandPoolCreateInfo {
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // flags
		m_QueueInfo.graphicsQueueIndex,                     // queueFamilyIndex
	};

	m_CommandPool           = m_LogicalDevice.createCommandPool(commandPoolCreateInfo, nullptr);
	m_ForwardPass.cmdPool   = m_LogicalDevice.createCommandPool(commandPoolCreateInfo, nullptr);
	m_UploadContext.cmdPool = m_LogicalDevice.createCommandPool(commandPoolCreateInfo, nullptr);

	vk::CommandBufferAllocateInfo uploadContextCmdBufferAllocInfo {
		m_UploadContext.cmdPool,
		vk::CommandBufferLevel::ePrimary,
		1u,
	};
	m_UploadContext.cmdBuffer = m_LogicalDevice.allocateCommandBuffers(uploadContextCmdBufferAllocInfo)[0];

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::CommandBufferAllocateInfo primaryCmdBufferAllocInfo {
			m_ForwardPass.cmdPool,            // commandPool
			vk::CommandBufferLevel::ePrimary, // level
			1ull,                             // commandBufferCount
		};

		vk::CommandBufferAllocateInfo secondaryCmdBuffersAllocInfo {
			m_CommandPool,                      // commandPool
			vk::CommandBufferLevel::eSecondary, // level
			MAX_FRAMES_IN_FLIGHT,               // commandBufferCount
		};

		m_ForwardPass.cmdBuffers[i].primary     = m_LogicalDevice.allocateCommandBuffers(primaryCmdBufferAllocInfo)[0];
		m_ForwardPass.cmdBuffers[i].secondaries = m_LogicalDevice.allocateCommandBuffers(secondaryCmdBuffersAllocInfo);
	}
}

void Renderer::CreateDescriptorResources(vk::PhysicalDevice physicalDevice)
{
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferCreateInfo createInfo {

			m_LogicalDevice,                                       // logicalDevice
			physicalDevice,                                        // physicalDevice
			m_Allocator,                                           // vmaallocator
			m_CommandPool,                                         // commandPool
			m_QueueInfo.graphicsQueue,                             // graphicsQueue
			vk::BufferUsageFlagBits::eUniformBuffer,               // usage
			(sizeof(glm::mat4) * 2ul) + (sizeof(glm::vec4) * 2ul), // size
			{}                                                     // inital data
		};

		m_Frames[i].cameraData.buffer = std::make_unique<Buffer>(createInfo);
	}
}

void Renderer::CreateFramePass()
{
	// Descriptor set layout
	std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
		// [0] UniformBuffer - View Projection
		vk::DescriptorSetLayoutBinding {
		    0ul,
		    vk::DescriptorType::eUniformBuffer,
		    1ul,
		    vk::ShaderStageFlagBits::eVertex,
		    nullptr,
		},
	};

	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {
		{},
		static_cast<uint32_t>(descriptorSetLayoutBindings.size()),
		descriptorSetLayoutBindings.data(),
	};
	m_FramesDescriptorSetLayout = m_LogicalDevice.createDescriptorSetLayout(descriptorSetLayoutCreateInfo, nullptr);

	// Allocate descriptor sets
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DescriptorSetAllocateInfo allocInfo {
			m_DescriptorPool,
			1ul,
			&m_FramesDescriptorSetLayout,
		};
		m_Frames[i].descriptorSet = m_LogicalDevice.allocateDescriptorSets(allocInfo)[0];
	}

	// Pipeline layout
	std::array<vk::DescriptorSetLayout, 1> setLayouts = {
		m_FramesDescriptorSetLayout,
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
		{},                // flags
		setLayouts.size(), // setLayoutCount
		setLayouts.data(), //// pSetLayouts
	};

	m_FramePipelineLayout = m_LogicalDevice.createPipelineLayout(pipelineLayoutCreateInfo, nullptr);
}

void Renderer::CreateForwardPass(const DeviceContext& deviceContext)
{
	// Descriptor set layout
	std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
		// [0] Sampler - Texture
		vk::DescriptorSetLayoutBinding {
		    0u,                                        // binding
		    vk::DescriptorType::eCombinedImageSampler, // descriptorType
		    32u,                                       // descriptorCount
		    vk::ShaderStageFlagBits::eFragment,        // stageFlags
		    nullptr,                                   // pImmutableSamplers
		},

		// [1] Sampler - Texture
		vk::DescriptorSetLayoutBinding {
		    1u,                                        // binding
		    vk::DescriptorType::eCombinedImageSampler, // descriptorType
		    8u,                                        // descriptorCount
		    vk::ShaderStageFlagBits::eFragment,        // stageFlags
		    nullptr,                                   // pImmutableSamplers
		},
	};

	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {
		{},                                                        // flags
		static_cast<uint32_t>(descriptorSetLayoutBindings.size()), // bindingCount
		descriptorSetLayoutBindings.data(),                        // pBindings
	};
	m_ForwardPass.descriptorSetLayout = m_LogicalDevice.createDescriptorSetLayout(descriptorSetLayoutCreateInfo, nullptr);


	// Allocate descriptor sets
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DescriptorSetAllocateInfo allocInfo {
			m_DescriptorPool,
			1ul,
			&m_ForwardPass.descriptorSetLayout,
		};

		m_ForwardPass.descriptorSets[i] = m_LogicalDevice.allocateDescriptorSets(allocInfo)[0];
	}

	// Pipeline layout
	std::array<vk::DescriptorSetLayout, 2> setLayouts = {
		m_FramesDescriptorSetLayout,
		m_ForwardPass.descriptorSetLayout,
	};
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
		{},                // flags
		setLayouts.size(), // setLayoutCount
		setLayouts.data(), //// pSetLayouts
	};

	m_ForwardPass.pipelineLayout = m_LogicalDevice.createPipelineLayout(pipelineLayoutCreateInfo, nullptr);

	// Storage buffer
	BufferCreateInfo createInfo {
		.logicalDevice  = m_LogicalDevice,
		.physicalDevice = deviceContext.physicalDevice,
		.allocator      = m_Allocator,
		.commandPool    = m_CommandPool,
		.graphicsQueue  = m_QueueInfo.graphicsQueue,
		.usage          = vk::BufferUsageFlagBits::eStorageBuffer,
		.size           = sizeof(glm::mat4) * 100,
		.initialData    = {},
	};

	m_ForwardPass.storageBuffer = std::make_unique<Buffer>(createInfo);
}

void Renderer::CreateUIPass(Window* window, const DeviceContext& deviceContext)
{
	ImGui::CreateContext();

	ImGui_ImplGlfw_InitForVulkan(window->GetGlfwHandle(), true);

	ImGui_ImplVulkan_InitInfo initInfo {
		.Instance              = deviceContext.instance,
		.PhysicalDevice        = deviceContext.physicalDevice,
		.Device                = m_LogicalDevice,
		.Queue                 = m_QueueInfo.graphicsQueue,
		.DescriptorPool        = m_DescriptorPool,
		.UseDynamicRendering   = true,
		.ColorAttachmentFormat = static_cast<VkFormat>(m_SurfaceInfo.format.format),
		.MinImageCount         = MAX_FRAMES_IN_FLIGHT,
		.ImageCount            = MAX_FRAMES_IN_FLIGHT,
		.MSAASamples           = static_cast<VkSampleCountFlagBits>(m_SampleCount),
	};

	std::pair userData = std::make_pair(deviceContext.vkGetInstanceProcAddr, deviceContext.instance);

	ASSERT(ImGui_ImplVulkan_LoadFunctions(
	           [](const char* func, void* data) {
		           auto [vkGetProcAddr, instance] = *(std::pair<PFN_vkGetInstanceProcAddr, vk::Instance>*)data;
		           return vkGetProcAddr(instance, func);
	           },
	           (void*)&userData),
	       "ImGui failed to load vulkan functions");

	ImGui_ImplVulkan_Init(&initInfo, VK_NULL_HANDLE);

	ImmediateSubmit([](vk::CommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
	});
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Renderer::WriteDescriptorSetsDefaultValues()
{
	std::vector<vk::WriteDescriptorSet> descriptorWrites;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DescriptorBufferInfo viewProjectionBufferInfo {
			*m_Frames[i].cameraData.buffer->GetBuffer(),
			0ul,
			VK_WHOLE_SIZE,
		};

		descriptorWrites.push_back({
		    vk::WriteDescriptorSet {
		        m_Frames[i].descriptorSet,
		        0ul,
		        0ul,
		        1ul,
		        vk::DescriptorType::eUniformBuffer,
		        nullptr,
		        &viewProjectionBufferInfo,
		        nullptr,
		    },
		});
	}

	for (uint32_t i = 0; i < 32; i++)
	{
		for (uint32_t j = 0; j < MAX_FRAMES_IN_FLIGHT; j++)
		{
			descriptorWrites.push_back(vk::WriteDescriptorSet {
			    m_ForwardPass.descriptorSets[j],
			    0ul,
			    i,
			    1ul,
			    vk::DescriptorType::eCombinedImageSampler,
			    &m_DefaultTexture->descriptorInfo,
			    nullptr,
			    nullptr,
			});
		}
	}

	for (uint32_t i = 0; i < 8; i++)
	{
		for (uint32_t j = 0; j < MAX_FRAMES_IN_FLIGHT; j++)
		{
			descriptorWrites.push_back(vk::WriteDescriptorSet {
			    m_ForwardPass.descriptorSets[j],
			    1ul,
			    i,
			    1ul,
			    vk::DescriptorType::eCombinedImageSampler,
			    &m_SkyboxTexture->descriptorInfo,
			    nullptr,
			    nullptr,
			});
		}
	}
	m_LogicalDevice.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0ull, nullptr);

	m_SwapchainInvalidated = false;
	m_LogicalDevice.waitIdle();
}


void Renderer::DestroySwapchain()
{
	if (!m_Swapchain)
	{
		return;
	}

	m_LogicalDevice.waitIdle();
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	m_LogicalDevice.resetCommandPool(m_CommandPool);
	m_LogicalDevice.resetCommandPool(m_UploadContext.cmdPool);
	m_LogicalDevice.destroyCommandPool(m_ForwardPass.cmdPool);
	m_LogicalDevice.destroyCommandPool(m_UploadContext.cmdPool);
	m_LogicalDevice.destroyCommandPool(m_CommandPool);

	m_LogicalDevice.resetDescriptorPool(m_DescriptorPool);

	for (const auto& imageView : m_ImageViews)
	{
		m_LogicalDevice.destroyImageView(imageView);
	}

	m_LogicalDevice.destroyImageView(m_DepthImageView);
	m_LogicalDevice.destroyImageView(m_ColorImageView);

	m_Allocator.destroyImage(m_DepthImage, m_DepthImage);
	m_Allocator.destroyImage(m_ColorImage, m_ColorImage);

	m_LogicalDevice.destroyDescriptorSetLayout(m_FramesDescriptorSetLayout);
	m_LogicalDevice.destroyDescriptorSetLayout(m_ForwardPass.descriptorSetLayout);

	m_LogicalDevice.destroyPipelineLayout(m_FramePipelineLayout);
	m_LogicalDevice.destroyPipelineLayout(m_ForwardPass.pipelineLayout);

	m_LogicalDevice.destroySwapchainKHR(m_Swapchain);
};

void Renderer::BeginFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();
	CVar::DrawImguiEditor();
}

void Renderer::DrawScene(Scene* scene, const Camera& camera)
{
	if (m_SwapchainInvalidated)
		return;

	const FrameData& frame = m_Frames[m_CurrentFrame];

	VKC(m_LogicalDevice.waitForFences(1u, &frame.renderFence, VK_TRUE, UINT64_MAX));

	uint32_t imageIndex;
	vk::Result result = m_LogicalDevice.acquireNextImageKHR(m_Swapchain, UINT64_MAX, frame.renderSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_SwapchainInvalidated)
	{
		m_LogicalDevice.waitIdle();
		m_SwapchainInvalidated = true;
		return;
	}
	else
	{
		ASSERT(result == vk::Result::eSuccess, "VkAcquireNextImage failed without returning VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR");
	}

	UpdateFrameDescriptorSet(frame, camera);
	UpdateForwardPassDescriptorSet(scene, m_CurrentFrame);

	const auto cmd = m_ForwardPass.cmdBuffers[m_CurrentFrame].primary;
	cmd.reset();

	cmd.begin(vk::CommandBufferBeginInfo {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_FramePipelineLayout, 0ul, 1ul, &frame.descriptorSet, 0ul, nullptr);
	SetupRenderBarriers(cmd, imageIndex);

	RenderForwardPass(scene, cmd, imageIndex);
	RenderUIPass(cmd, imageIndex);

	SetupPresentBarriers(cmd, imageIndex);
	cmd.end();

	SubmitQueue(frame, cmd);
	PresentFrame(frame, imageIndex);

	m_CurrentFrame = (m_CurrentFrame + 1u) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::UpdateFrameDescriptorSet(const FrameData& frame, const Camera& camera)
{
	struct PerFrameData
	{
		glm::mat4 projection;
		glm::mat4 view;
		glm::vec4 lightPos;
		glm::vec4 viewPos;
	};

	PerFrameData* map = (PerFrameData*)frame.cameraData.buffer->Map();
	map->projection   = camera.GetProjection();
	map->view         = camera.GetView();
	map->lightPos     = glm::vec4(2.0f, 2.0f, 1.0f, 1.0f);
	map->viewPos      = camera.GetPosition();
	frame.cameraData.buffer->Unmap();
}

void Renderer::UpdateForwardPassDescriptorSet(Scene* scene, uint32_t frameIndex)
{
	scene->GetRegistry()->group(entt::get<TransformComponent, StaticMeshRendererComponent>).each([&](TransformComponent& transformComp, StaticMeshRendererComponent& renderComp) {
		for (const auto* node : renderComp.model->nodes)
		{
			for (const auto& primitive : node->mesh)
			{
				const auto& model    = renderComp.model;
				const auto& material = model->materialParameters[primitive.materialIndex];

				uint32_t albedoTextureIndex            = material.albedoTextureIndex;
				uint32_t normalTextureIndex            = material.normalTextureIndex;
				uint32_t metallicRoughnessTextureIndex = material.metallicRoughnessTextureIndex;

				Texture* albedoTexture            = model->textures[albedoTextureIndex];
				Texture* normalTexture            = model->textures[normalTextureIndex];
				Texture* metallicRoughnessTexture = model->textures[metallicRoughnessTextureIndex];

				std::vector<vk::WriteDescriptorSet> descriptorWrites = {
					vk::WriteDescriptorSet {
					    m_ForwardPass.descriptorSets[frameIndex],
					    0ul,
					    albedoTextureIndex,
					    1ul,
					    vk::DescriptorType::eCombinedImageSampler,
					    &albedoTexture->descriptorInfo,
					    nullptr,
					    nullptr,
					},
					vk::WriteDescriptorSet {
					    m_ForwardPass.descriptorSets[frameIndex],
					    0ul,
					    metallicRoughnessTextureIndex,
					    1ul,
					    vk::DescriptorType::eCombinedImageSampler,
					    &metallicRoughnessTexture->descriptorInfo,
					    nullptr,
					    nullptr,
					},
					vk::WriteDescriptorSet {
					    m_ForwardPass.descriptorSets[frameIndex],
					    0ul,
					    normalTextureIndex,
					    1ul,
					    vk::DescriptorType::eCombinedImageSampler,
					    &normalTexture->descriptorInfo,
					    nullptr,
					    nullptr,
					}
				};

				m_LogicalDevice.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0ull, nullptr);
			}
		}
	});
}

void Renderer::SetupRenderBarriers(vk::CommandBuffer cmd, uint32_t imageIndex)
{
	// Transition image to color write
	vk::ImageMemoryBarrier imageMemoryBarrier {
		{},                                        // srcAccessMask
		vk::AccessFlagBits::eColorAttachmentWrite, // dstAccessMask
		vk::ImageLayout::eUndefined,               // oldLayout
		vk::ImageLayout::eColorAttachmentOptimal,  // newLayout
		{},
		{},
		m_Images[imageIndex], // image

		/* subresourceRange */
		vk::ImageSubresourceRange {
		    vk::ImageAspectFlagBits::eColor,
		    0u,
		    1u,
		    0u,
		    1u,
		},
	};

	cmd.pipelineBarrier(
	    vk::PipelineStageFlagBits::eTopOfPipe,
	    vk::PipelineStageFlagBits::eColorAttachmentOutput,
	    {},
	    {},
	    {},
	    imageMemoryBarrier);
}

void Renderer::SetupPresentBarriers(vk::CommandBuffer cmd, uint32_t imageIndex)
{
	// Transition color image to present optimal
	vk::ImageMemoryBarrier imageMemoryBarrier = {
		vk::AccessFlagBits::eColorAttachmentWrite, // srcAccessMask
		{},                                        // dstAccessMask
		vk::ImageLayout::eColorAttachmentOptimal,  // oldLayout
		vk::ImageLayout::ePresentSrcKHR,           // newLayout
		{},
		{},
		m_Images[imageIndex], // image

		/* subresourceRange */
		vk::ImageSubresourceRange {
		    vk::ImageAspectFlagBits::eColor,
		    0u,
		    1u,
		    0u,
		    1u,
		},
	};

	cmd.pipelineBarrier(
	    vk::PipelineStageFlagBits::eColorAttachmentOutput,
	    vk::PipelineStageFlagBits::eBottomOfPipe,
	    {},
	    {},
	    {},
	    imageMemoryBarrier);
}

void Renderer::RenderForwardPass(Scene* scene, vk::CommandBuffer cmd, uint32_t imageIndex)
{
	vk::RenderingAttachmentInfo colorAttachmentInfo {
		m_ColorImageView,                         // imageView
		vk::ImageLayout::eColorAttachmentOptimal, // imageLayout

		vk::ResolveModeFlagBits::eAverage,        // resolveMode
		m_ImageViews[imageIndex],                 // resolveImageView
		vk::ImageLayout::eColorAttachmentOptimal, // resolveImageLayout

		vk::AttachmentLoadOp::eClear,  // loadOp
		vk::AttachmentStoreOp::eStore, // storeOp

		/* clearValue */
		vk::ClearColorValue {
		    std::array<float, 4> { 0.0f, 0.0f, 0.0f, 1.0f },
		},
	};

	vk::RenderingAttachmentInfo depthAttachmentInfo {
		m_DepthImageView,                                // imageView
		vk::ImageLayout::eDepthStencilAttachmentOptimal, // imageLayout

		{}, // resolveMode
		{}, // resolveImageView
		{}, // resolveImageLayout

		vk::AttachmentLoadOp::eClear,  // loadOp
		vk::AttachmentStoreOp::eStore, // storeOp

		/* clearValue */
		vk::ClearDepthStencilValue {
		    { 1.0, 0 },
		},
	};

	vk::RenderingInfo renderingInfo {
		{}, // flags

		/* renderArea */
		vk::Rect2D {
		    { 0, 0 },                                 // offset
		    m_SurfaceInfo.capabilities.currentExtent, // extent
		},

		1u,                   // layerCount
		{},                   // viewMask
		1u,                   // colorAttachmentCount
		&colorAttachmentInfo, // pColorAttachments
		&depthAttachmentInfo, // pDepthAttachment
		{},                   // pStencilAttachment
	};

	cmd.beginRendering(renderingInfo);

	const vk::DescriptorSet descriptorSet = m_ForwardPass.descriptorSets[m_CurrentFrame];

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_ForwardPass.pipelineLayout, 1ul, 1ul, &m_ForwardPass.descriptorSets[m_CurrentFrame], 0ul, nullptr);

	VkDeviceSize offset { 0 };

	vk::Pipeline currentPipeline = VK_NULL_HANDLE;
	uint32_t primitives          = 0;
	scene->GetRegistry()->group(entt::get<TransformComponent, StaticMeshRendererComponent>).each([&](TransformComponent& transformComp, StaticMeshRendererComponent& renderComp) {
		// bind pipeline
		vk::Pipeline newPipeline = renderComp.material->base->shader->pipeline;
		if (currentPipeline != newPipeline)
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, newPipeline);
			currentPipeline = newPipeline;
		}

		// bind buffers
		cmd.bindVertexBuffers(0, 1, renderComp.model->vertexBuffer->GetBuffer(), &offset);
		cmd.bindIndexBuffer(*(renderComp.model->indexBuffer->GetBuffer()), 0u, vk::IndexType::eUint32);

		// draw primitves
		for (const auto* node : renderComp.model->nodes)
		{
			for (const auto& primitive : node->mesh)
			{
				uint32_t textureIndex = renderComp.model->materialParameters[primitive.materialIndex].albedoTextureIndex;
				cmd.drawIndexed(primitive.indexCount, 1ull, primitive.firstIndex, 0ull, textureIndex); // @todo: lol fix this garbage
			}
		}
	});


	cmd.endRendering();
}

void Renderer::RenderUIPass(vk::CommandBuffer cmd, uint32_t imageIndex)
{
	ImGui::Render();

	vk::RenderingAttachmentInfo colorAttachmentInfo = {
		m_ColorImageView,                         // imageView
		vk::ImageLayout::eColorAttachmentOptimal, // imageLayout

		vk::ResolveModeFlagBits::eAverage,        // resolveMode
		m_ImageViews[imageIndex],                 // resolveImageView
		vk::ImageLayout::eColorAttachmentOptimal, // resolveImageLayout

		vk::AttachmentLoadOp::eLoad,   // loadOp
		vk::AttachmentStoreOp::eStore, // storeOp
	};
	vk::RenderingInfo renderingInfo = {
		{}, // flags

		/* renderArea */
		vk::Rect2D {
		    { 0, 0 },                                 // offset
		    m_SurfaceInfo.capabilities.currentExtent, // extent
		},

		1u,                   // layerCount
		{},                   // viewMask
		1u,                   // colorAttachmentCount
		&colorAttachmentInfo, // pColorAttachments
		{},                   // pDepthAttachment
		{},                   // pStencilAttachment
	};

	cmd.beginRendering(renderingInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	cmd.endRendering();
}


void Renderer::SubmitQueue(const FrameData& frame, vk::CommandBuffer cmd)
{
	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	vk::SubmitInfo submitInfo {
		1u,                      // waitSemaphoreCount
		&frame.renderSemaphore,  // pWaitSemaphores
		&waitStage,              // pWaitDstStageMask
		1u,                      // commandBufferCount
		&cmd,                    // pCommandBuffers
		1u,                      // signalSemaphoreCount
		&frame.presentSemaphore, // pSignalSemaphores
	};

	VKC(m_LogicalDevice.resetFences(1u, &frame.renderFence));
	VKC(m_QueueInfo.graphicsQueue.submit(1u, &submitInfo, frame.renderFence));
}

void Renderer::PresentFrame(const FrameData& frame, uint32_t imageIndex)
{
	vk::PresentInfoKHR presentInfo {
		1u,                      // waitSemaphoreCount
		&frame.presentSemaphore, // pWaitSemaphores
		1u,                      // swapchainCount
		&m_Swapchain,            // pSwapchains
		&imageIndex,             // pImageIndices
		nullptr                  // pResults
	};

	try
	{
		vk::Result result = m_QueueInfo.presentQueue.presentKHR(presentInfo);
		if (result == vk::Result::eSuboptimalKHR || m_SwapchainInvalidated)
		{
			m_LogicalDevice.waitIdle();
			m_SwapchainInvalidated = true;
			return;
		}
	}
	catch (vk::OutOfDateKHRError err) // OutOfDateKHR is not considered a success value and throws an error (presentKHR)
	{
		m_LogicalDevice.waitIdle();
		m_SwapchainInvalidated = true;
		return;
	}
}


void Renderer::ImmediateSubmit(std::function<void(vk::CommandBuffer)>&& function)
{
	vk::CommandBuffer cmd = m_UploadContext.cmdBuffer;

	vk::CommandBufferBeginInfo beginInfo {
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	};

	cmd.begin(beginInfo);

	function(cmd);

	cmd.end();

	vk::SubmitInfo submitInfo { 0u, {}, {}, 1u, &cmd, 0u, {}, {} };

	m_QueueInfo.graphicsQueue.submit(submitInfo, m_UploadContext.fence);
	VKC(m_LogicalDevice.waitForFences(m_UploadContext.fence, true, UINT_MAX));
	m_LogicalDevice.resetFences(m_UploadContext.fence);

	m_LogicalDevice.resetCommandPool(m_CommandPool, {});
}
