#include "Graphics/RenderGraph.hpp"

#include <vulkan/vk_enum_string_helper.h>

RenderGraph::RenderGraph()
{
}

RenderGraph::~RenderGraph()
{
}

void RenderGraph::Init(const CreateInfo& info)
{
	m_SwapchainImageCount = info.swapchainImageCount;
	m_SwapchainExtent     = info.swapchainExtent;
	m_LogicalDevice       = info.logicalDevice;
	m_DescriptorPool      = info.descriptorPool;
	m_PhysicalDevice      = info.physicalDevice;
	m_Allocator           = info.allocator;
	m_CommandPool         = info.commandPool;
	m_QueueInfo           = info.queueInfo;
	m_ColorFormat         = info.colorFormat;
	m_DepthFormat         = info.depthFormat;

	m_MinUniformBufferOffsetAlignment = m_PhysicalDevice.getProperties().limits.minUniformBufferOffsetAlignment;
}

void RenderGraph::Build()
{
	m_RenderPasses.resize(m_Recipes.size(), RenderPass {});

	ValidateGraph();

	ReorderPasses();

	BuildAttachmentResources();

	BuildTextureInputs();

	BuildBufferInputs();

	BuildDescriptorSets();

	WriteDescriptorSets();
}

// @todo: Implement
void RenderGraph::ValidateGraph()
{
}

// @todo: Implement
void RenderGraph::ReorderPasses()
{
	for (uint32_t i = m_Recipes.size(); i-- > 0;)
	{
		auto& recipe = m_Recipes[i];


		for (const RenderPassRecipe::AttachmentInfo& info : recipe.colorAttachmentInfos)
		{
			auto it = std::find(m_BackbufferAttachmentNames.begin(),
			                    m_BackbufferAttachmentNames.end(),
			                    info.name);

			if (it != m_BackbufferAttachmentNames.end())
			{
				m_BackbufferAttachmentNames.push_back(info.input);
			}
		}
	}
}

void RenderGraph::BuildAttachmentResources()
{
	uint32_t passIndex = 0u;
	for (const RenderPassRecipe& recipe : m_Recipes)
	{
		RenderPass& pass = m_RenderPasses[passIndex++];

		for (const RenderPassRecipe::AttachmentInfo& info : recipe.colorAttachmentInfos)
		{
			// Write only ( create new image )
			if (info.input == "")
			{
				auto it = std::find(m_BackbufferAttachmentNames.begin(),
				                    m_BackbufferAttachmentNames.end(),
				                    info.input);


				CreateAttachmentResource(info, it != m_BackbufferAttachmentNames.end() ?
				                                   AttachmentResource::Type::ePerImage :
				                                   AttachmentResource::Type::eSingle);

				pass.attachments.push_back({
				    .stageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
				    .accessMask = vk::AccessFlagBits::eColorAttachmentWrite,
				    .layout     = vk::ImageLayout::eColorAttachmentOptimal,
				    /* subresourceRange */
				    .subresourceRange = vk::ImageSubresourceRange {
				        vk::ImageAspectFlagBits::eColor, // aspectMask
				        0u,                              // baseMipLevel
				        1u,                              // levelCount
				        0u,                              // baseArrayLayer
				        1u,                              // layerCount
				    },

				    .loadOp  = vk::AttachmentLoadOp::eClear,
				    .storeOp = vk::AttachmentStoreOp::eStore,
				});
			}
			// Read-Modify-Write ( alias the image )
			else
			{
				uint32_t i = 0;
				for (AttachmentResource& attachmentResource : m_AttachmentResources)
				{
					if (attachmentResource.size != info.size || attachmentResource.sizeType != info.sizeType)
					{
						ASSERT(false, "ReadWrite attachment with different size from input is currently not supported ");
					}

					if (attachmentResource.lastWriteName == info.input)
					{
						attachmentResource.lastWriteName = info.name;

						pass.attachments.push_back({
						    .stageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
						    .accessMask = vk::AccessFlagBits::eColorAttachmentWrite,
						    .layout     = vk::ImageLayout::eColorAttachmentOptimal,
						    /* subresourceRange */
						    .subresourceRange = vk::ImageSubresourceRange {
						        vk::ImageAspectFlagBits::eColor, // aspectMask
						        0u,                              // baseMipLevel
						        1u,                              // levelCount
						        0u,                              // baseArrayLayer
						        1u,                              // layerCount
						    },

						    .loadOp  = vk::AttachmentLoadOp::eLoad,
						    .storeOp = vk::AttachmentStoreOp::eStore,
						});
						break;
					}

					i++;
				}
			}
		}

		if (!recipe.depthStencilAttachmentInfo.name.empty())
		{
			const RenderPassRecipe::AttachmentInfo& info = recipe.depthStencilAttachmentInfo;
			// Write only ( create new image )
			if (info.input == "")
			{
				auto it = std::find(m_BackbufferAttachmentNames.begin(),
				                    m_BackbufferAttachmentNames.end(),
				                    info.input);


				CreateAttachmentResource(info, AttachmentResource::Type::eSingle);

				pass.attachments.push_back({
				    .stageMask  = vk::PipelineStageFlagBits::eEarlyFragmentTests,
				    .accessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead,
				    .layout     = vk::ImageLayout::eDepthAttachmentOptimal,
				    /* subresourceRange */
				    .subresourceRange = vk::ImageSubresourceRange {
				        vk::ImageAspectFlagBits::eDepth, // aspectMask
				        0u,                              // baseMipLevel
				        1u,                              // levelCount
				        0u,                              // baseArrayLayer
				        1u,                              // layerCount
				    },

				    .loadOp  = vk::AttachmentLoadOp::eClear,
				    .storeOp = vk::AttachmentStoreOp::eStore,
				});
			}
			// Read-Modify-Write ( alias the image )
			else
			{
				uint32_t i = 0;
				for (AttachmentResource& attachmentResource : m_AttachmentResources)
				{
					if (attachmentResource.size != info.size || attachmentResource.sizeType != info.sizeType)
					{
						ASSERT(false, "ReadWrite attachment with different size from input is currently not supported ");
					}

					if (attachmentResource.lastWriteName == info.input)
					{
						attachmentResource.lastWriteName = info.name;

						pass.attachments.push_back({
						    .stageMask  = vk::PipelineStageFlagBits::eEarlyFragmentTests,
						    .accessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead,
						    .layout     = vk::ImageLayout::eDepthAttachmentOptimal,
						    /* subresourceRange */
						    .subresourceRange = vk::ImageSubresourceRange {
						        vk::ImageAspectFlagBits::eDepth, // aspectMask
						        0u,                              // baseMipLevel
						        1u,                              // levelCount
						        0u,                              // baseArrayLayer
						        1u,                              // layerCount
						    },

						    .loadOp  = vk::AttachmentLoadOp::eLoad,
						    .storeOp = vk::AttachmentStoreOp::eStore,
						});
						break;
					}

					i++;
				}
			}
		}
	}
}


void RenderGraph::BuildTextureInputs()
{
}

void RenderGraph::BuildBufferInputs()
{
	for (RenderPassRecipe::BufferInputInfo& info : m_BufferInputInfos)
	{
		info.size = (info.size + m_MinUniformBufferOffsetAlignment - 1) & -m_MinUniformBufferOffsetAlignment;

		BufferCreateInfo bufferCreateInfo {
			.logicalDevice = m_LogicalDevice,
			.allocator     = m_Allocator,
			.commandPool   = m_CommandPool,
			.graphicsQueue = m_QueueInfo.graphicsQueue,
			.usage         = info.type == vk::DescriptorType::eUniformBuffer ? vk::BufferUsageFlagBits::eUniformBuffer :
			                                                                   vk::BufferUsageFlagBits::eStorageBuffer,
			.size          = info.size * MAX_FRAMES_IN_FLIGHT,
		};

        LOG(err, "Creating bfinpt: {}", info.name);
		m_BufferInputs.emplace(HashStr(info.name.c_str()), new Buffer(bufferCreateInfo));
	}

	uint32_t passIndex = 0u;
	for (RenderPassRecipe& recipe : m_Recipes)
	{
		RenderPass& pass = m_RenderPasses[passIndex++];

		for (RenderPassRecipe::BufferInputInfo& info : recipe.bufferInputInfos)
		{
			info.size = (info.size + m_MinUniformBufferOffsetAlignment - 1) & -m_MinUniformBufferOffsetAlignment;
			BufferCreateInfo bufferCreateInfo {
				.logicalDevice = m_LogicalDevice,
				.allocator     = m_Allocator,
				.commandPool   = m_CommandPool,
				.graphicsQueue = m_QueueInfo.graphicsQueue,
				.usage         = info.type == vk::DescriptorType::eUniformBuffer ? vk::BufferUsageFlagBits::eUniformBuffer :
				                                                                   vk::BufferUsageFlagBits::eStorageBuffer,
				.size          = info.size * MAX_FRAMES_IN_FLIGHT,
			};

			pass.bufferInputs.emplace(HashStr(info.name.c_str()), new Buffer(bufferCreateInfo));
		}
	}
}

void RenderGraph::BuildDescriptorSets()
{
	{
		std::vector<vk::DescriptorSetLayoutBinding> bindings;
		for (const RenderPassRecipe::BufferInputInfo& info : m_BufferInputInfos)
		{
			// Determine bindings
			bindings.push_back(vk::DescriptorSetLayoutBinding {
			    info.binding,
			    info.type,
			    info.count,
			    info.stageMask,
			});
		}
		// Create descriptor set layout
		vk::DescriptorSetLayoutCreateInfo layoutCreateInfo {
			{}, // flags
			static_cast<uint32_t>(bindings.size()),
			bindings.data(),

		};
		m_DescriptorSetLayout = m_LogicalDevice.createDescriptorSetLayout(layoutCreateInfo);

		// Allocate descriptor sets
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorSetAllocateInfo allocInfo {
				m_DescriptorPool,
				1,
				&m_DescriptorSetLayout,
			};
			m_DescriptorSets.push_back(m_LogicalDevice.allocateDescriptorSets(allocInfo)[0]);
		}

		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
			{},
			1ull,                   // setLayoutCount
			&m_DescriptorSetLayout, // pSetLayouts
		};

		m_PipelineLayout = m_LogicalDevice.createPipelineLayout(pipelineLayoutCreateInfo);
	}

	uint32_t passIndex = 0u;
	for (const RenderPassRecipe& recipe : m_Recipes)
	{
		RenderPass& pass = m_RenderPasses[passIndex++];

		// Determine bindings
		std::vector<vk::DescriptorSetLayoutBinding> bindings;
		for (const RenderPassRecipe::BufferInputInfo info : recipe.bufferInputInfos)
		{
			bindings.push_back(vk::DescriptorSetLayoutBinding {
			    info.binding,
			    info.type,
			    info.count,
			    info.stageMask,
			});
		}
		for (const RenderPassRecipe::TextureInputInfo info : recipe.textureInputInfos)
		{
			bindings.push_back(vk::DescriptorSetLayoutBinding {
			    info.binding,
			    info.type,
			    info.count,
			    info.stageMask,
			});
		}

		// Create descriptor set layout
		vk::DescriptorSetLayoutCreateInfo layoutCreateInfo {
			{}, // flags
			static_cast<uint32_t>(bindings.size()),
			bindings.data(),

		};
		pass.descriptorSetLayout = m_LogicalDevice.createDescriptorSetLayout(layoutCreateInfo);

		// Allocate descriptor sets
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorSetAllocateInfo allocInfo {
				m_DescriptorPool,
				1,
				&pass.descriptorSetLayout,
			};
			pass.descriptorSets.push_back(m_LogicalDevice.allocateDescriptorSets(allocInfo)[0]);
		}

		// Create pipeline layout
		std::array<vk::DescriptorSetLayout, 2> layouts {
			m_DescriptorSetLayout,
			pass.descriptorSetLayout
		};

		vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
			{},
			2ull,           // setLayoutCount
			layouts.data(), // pSetLayouts
		};

		pass.pipelineLayout = m_LogicalDevice.createPipelineLayout(pipelineLayoutCreateInfo);
	}
}

void RenderGraph::WriteDescriptorSets()
{
	{
		std::vector<vk::DescriptorBufferInfo> bufferInfos;
		bufferInfos.reserve(m_BufferInputs.size() * MAX_FRAMES_IN_FLIGHT);
		for (const RenderPassRecipe::BufferInputInfo info : m_BufferInputInfos)
		{
			std::vector<vk::WriteDescriptorSet> writes;

			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				bufferInfos.push_back({
				    *(m_BufferInputs[HashStr(info.name.c_str())]->GetBuffer()),
				    info.size * i,
				    info.size,
				});


				for (uint32_t j = 0; j < info.count; j++)
				{
					writes.push_back(vk::WriteDescriptorSet {
					    m_DescriptorSets[i], // dstSet
					    info.binding,
					    j,
					    1u,
					    info.type,
					    nullptr,
					    &bufferInfos.back(),
					});
				}
			}

			m_LogicalDevice.updateDescriptorSets(
			    static_cast<uint32_t>(writes.size()),
			    writes.data(),
			    0u,
			    nullptr);

			m_LogicalDevice.waitIdle();
		}
	}


	uint32_t passIndex = 0u;
	for (const RenderPassRecipe& recipe : m_Recipes)
	{
		RenderPass& pass = m_RenderPasses[passIndex++];

		std::vector<vk::WriteDescriptorSet> writes;

		std::vector<vk::DescriptorBufferInfo> bufferInfos;
		bufferInfos.reserve(pass.bufferInputs.size() * MAX_FRAMES_IN_FLIGHT);
		for (const RenderPassRecipe::BufferInputInfo info : recipe.bufferInputInfos)
		{
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				bufferInfos.push_back({
				    *(pass.bufferInputs[HashStr(info.name.c_str())]->GetBuffer()),
				    info.size * i,
				    info.size,
				});

				for (uint32_t j = 0; j < info.count; j++)
				{
					writes.push_back(vk::WriteDescriptorSet {
					    pass.descriptorSets[i], // dstSet
					    info.binding,
					    j,
					    1u,
					    info.type,
					    nullptr,
					    &bufferInfos.back(),
					});
				}
			}
		}
		for (const RenderPassRecipe::TextureInputInfo info : recipe.textureInputInfos)
		{
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			{
				for (uint32_t j = 0; j < info.count; j++)
				{
					writes.push_back(vk::WriteDescriptorSet {
					    pass.descriptorSets[i], // dstSet
					    info.binding,
					    j,
					    1u,
					    info.type,
					    &info.defaultTexture->descriptorInfo,
					    nullptr,
					});
				}
			}
		}

		m_LogicalDevice.updateDescriptorSets(
		    static_cast<uint32_t>(writes.size()),
		    writes.data(),
		    0u,
		    nullptr);

		m_LogicalDevice.waitIdle();
	}
}

void RenderGraph::CreateAttachmentResource(const RenderPassRecipe::AttachmentInfo& info, RenderGraph::AttachmentResource::Type type)
{
	LOG(trace, "Creating image: {} <- ", info.name, info.input);
	vk::ImageUsageFlags usage;
	vk::ImageAspectFlags aspectMask;
	if (info.format == m_ColorFormat)
	{
		usage      = vk::ImageUsageFlagBits::eColorAttachment;
		aspectMask = vk::ImageAspectFlagBits::eColor;
	}
	else if (info.format == m_DepthFormat)
	{
		usage      = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		aspectMask = vk::ImageAspectFlagBits::eDepth;
	}
	else
	{
		ASSERT(false, "Unsupported render attachment format: {}", string_VkFormat(static_cast<VkFormat>(info.format)));
	}

	// Determine image extent
	vk::Extent3D extent;
	switch (info.sizeType)
	{
	case RenderPassRecipe::SizeType::eSwapchainRelative:
		extent = {
			.width  = static_cast<uint32_t>(m_SwapchainExtent.width * info.size.x),
			.height = static_cast<uint32_t>(m_SwapchainExtent.height * info.size.y),
			.depth  = 1,
		};
		break;
	case RenderPassRecipe::SizeType::eRelative:
		ASSERT(false, "Unimplemented attachment size type: Relative");
		break;

	case RenderPassRecipe::SizeType::eAbsolute:
		extent = {
			.width  = static_cast<uint32_t>(info.size.x),
			.height = static_cast<uint32_t>(info.size.y),
			.depth  = 1,
		};
		break;

	default:
		ASSERT(false, "Invalid attachment size type");
	};

	AttachmentResource resource = {
		.lastWriteName = info.name,

		.accessMask = {},
		.stageMask  = vk::PipelineStageFlagBits::eTopOfPipe,
		.layout     = vk::ImageLayout::eUndefined,
		.format     = info.format,

		.extent           = extent,
		.size             = info.size,
		.sizeType         = info.sizeType,
		.relativeSizeName = info.sizeRelativeName,

		.images     = {},
		.imageViews = {},

		.sampleCount            = info.samples,
		.transientMSResolveMode = {},
		.transientMSImage       = {},
		.transientMSImageView   = {},
	};

	switch (type)
	{
	case AttachmentResource::Type::ePerImage:
	{
		resource.images     = m_SwapchainImages;
		resource.imageViews = m_SwapchainImageViews;
		break;
	}
	case AttachmentResource::Type::eSingle:
	{
		vk::ImageCreateInfo imageCreateInfo {
			{},                          // flags
			vk::ImageType::e2D,          // imageType
			info.format,                 // format
			extent,                      // extent
			1u,                          // mipLevels
			1u,                          // arrayLayers
			vk::SampleCountFlagBits::e1, // samples
			vk::ImageTiling::eOptimal,   // tiling
			usage,                       // usage
			vk::SharingMode::eExclusive, // sharingMode
			0u,                          // queueFamilyIndexCount
			nullptr,                     // pQueueFamilyIndices
			vk::ImageLayout::eUndefined, // initialLayout
		};

		vma::AllocationCreateInfo imageAllocInfo({}, vma::MemoryUsage::eGpuOnly, vk::MemoryPropertyFlagBits::eDeviceLocal);
		LOG(trace, "69696969");
		AllocatedImage image = m_Allocator.createImage(imageCreateInfo, imageAllocInfo);

		vk::ImageViewCreateInfo imageViewCreateInfo {
			{},                     // flags
			image,                  // image
			vk::ImageViewType::e2D, // viewType
			info.format,            // format

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
			    aspectMask, // aspectMask
			    0u,         // baseMipLevel
			    1u,         // levelCount
			    0u,         // baseArrayLayer
			    1u,         // layerCount
			},
		};
		vk::ImageView imageView = m_LogicalDevice.createImageView(imageViewCreateInfo, nullptr);

		resource.images     = { image };
		resource.imageViews = { imageView };
		break;
	}
	default:
		ASSERT(false, "Invalid attachment resource type");
	}

	if (static_cast<uint32_t>(info.samples) > 1)
	{
		vk::ImageCreateInfo imageCreateInfo {
			{},                        // flags
			vk::ImageType::e2D,        // imageType
			info.format,               // format
			extent,                    // extent
			1u,                        // mipLevels
			1u,                        // arrayLayers
			info.samples,              // samples
			vk::ImageTiling::eOptimal, // tiling

			usage | vk::ImageUsageFlagBits::eTransientAttachment, // usage

			vk::SharingMode::eExclusive, // sharingMode
			0u,                          // queueFamilyIndexCount
			nullptr,                     // pQueueFamilyIndices
			vk::ImageLayout::eUndefined, // initialLayout
		};

		vma::AllocationCreateInfo imageAllocInfo({}, vma::MemoryUsage::eGpuOnly, vk::MemoryPropertyFlagBits::eDeviceLocal);
		LOG(trace, "ABABABABAB");
		AllocatedImage image = m_Allocator.createImage(imageCreateInfo, imageAllocInfo);

		vk::ImageViewCreateInfo imageViewCreateInfo {
			{},                     // flags
			image,                  // image
			vk::ImageViewType::e2D, // viewType
			info.format,            // format

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
			    aspectMask, // aspectMask
			    0u,         // baseMipLevel
			    1u,         // levelCount
			    0u,         // baseArrayLayer
			    1u,         // layerCount
			},
		};
		vk::ImageView imageView = m_LogicalDevice.createImageView(imageViewCreateInfo, nullptr);

		resource.transientMSImage     = image;
		resource.transientMSImageView = imageView;
	}
}

void RenderGraph::Update(Scene* scene, uint32_t frameIndex)
{
	m_UpdateAction({
	    .graph         = *this,
	    .frameIndex    = frameIndex,
	    .logicalDevice = m_LogicalDevice,
	    .scene         = scene,
	});

	for (RenderPass& pass : m_RenderPasses)
	{
		LOG(critical, "{}", pass.name);
		pass.updateAction({
		    .renderPass    = pass,
		    .frameIndex    = frameIndex,
		    .logicalDevice = m_LogicalDevice,
		    .scene         = scene,
		});
	}
}

void RenderGraph::Render(const RenderPass::RenderData& data)
{
	// @todo: Setup render barriers
	const auto cmd = data.cmd;

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
	                       m_PipelineLayout,
	                       0u, 1ul, &m_DescriptorSets[data.frameIndex], 0ul, {});

	size_t i = 0;
	for (RenderPass& pass : m_RenderPasses)
	{
		std::vector<vk::RenderingAttachmentInfo> renderingColorAttachmentInfos;
		vk::RenderingAttachmentInfo renderingDepthAttachmentInfo;

		for (RenderPass::Attachment attachment : pass.attachments)
		{
			auto& resource = m_AttachmentResources[attachment.resourceIndex];

			auto [image, imageView, transientMSImage, transientMSImageView] =
			    resource.GetResource(data.imageIndex, data.frameIndex);

			vk::ImageMemoryBarrier imageBarrier = {
				resource.accessMask,            // srcAccessMask
				attachment.accessMask,          // dstAccessMask
				resource.layout,                // oldLayout
				attachment.layout,              // newLayout
				m_QueueInfo.graphicsQueueIndex, // srcQueueFamilyIndex
				m_QueueInfo.graphicsQueueIndex, // dstQueueFamilyIndex
				image,                          // image
				attachment.subresourceRange,    // subresourceRange
				nullptr,                        // pNext
			};

			cmd.pipelineBarrier(resource.stageMask,
			                    attachment.stageMask,
			                    {},
			                    {},
			                    {},
			                    imageBarrier);

			vk::RenderingAttachmentInfo renderingAttachmentInfo {};
			// Multi-sampled image
			if (static_cast<uint32_t>(resource.sampleCount) > 1)
			{
				vk::ImageMemoryBarrier transientMSImageBarrier = {
					resource.accessMask,            // srcAccessMask
					attachment.accessMask,          // dstAccessMask
					resource.layout,                // oldLayout
					attachment.layout,              // newLayout
					m_QueueInfo.graphicsQueueIndex, // srcQueueFamilyIndex
					m_QueueInfo.graphicsQueueIndex, // dstQueueFamilyIndex
					transientMSImage,               // image
					attachment.subresourceRange,    // subresourceRange
					nullptr,                        // pNext
				};

				cmd.pipelineBarrier(resource.stageMask,
				                    attachment.stageMask,
				                    {},
				                    {},
				                    {},
				                    transientMSImageBarrier);

				renderingAttachmentInfo = {
					transientMSImageView,
					attachment.layout,

					resource.transientMSResolveMode,
					imageView,
					attachment.layout,

					attachment.loadOp,
					attachment.storeOp,

					attachment.clearValue,
				};
			}
			// Single-sampled image
			else
			{
				renderingAttachmentInfo = {
					imageView,
					attachment.layout,

					vk::ResolveModeFlagBits::eNone,
					{},
					{},

					attachment.loadOp,
					attachment.storeOp,

					attachment.clearValue,
				};
			}

			if (attachment.subresourceRange.aspectMask & vk::ImageAspectFlagBits::eDepth)
			{
				renderingDepthAttachmentInfo = renderingAttachmentInfo;
			}
			else
			{
				renderingColorAttachmentInfos.push_back(renderingAttachmentInfo);
			}
		}

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
		                       pass.pipelineLayout,
		                       1ul, 1u, &pass.descriptorSets[data.frameIndex], 0ul, {});


		vk::RenderingInfo renderingInfo {
			{}, // flags
			vk::Rect2D {
			    { 0, 0 },
			    m_SwapchainExtent,
			},
			1u,
			{},
			static_cast<uint32_t>(renderingColorAttachmentInfos.size()),
			renderingColorAttachmentInfos.data(),
			&renderingDepthAttachmentInfo,
			{},
		};

		cmd.beginRendering(renderingInfo);
		pass.renderAction(data);
		cmd.endRendering();
		i++;
	}
}
