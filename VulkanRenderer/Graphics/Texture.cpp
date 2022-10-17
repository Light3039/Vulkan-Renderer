#include "Graphics/Texture.hpp"

#include <AssetParser.hpp>
#include <TextureAsset.hpp>
#include <tiny_gltf.h>

TextureSystem::TextureSystem(const TextureSystem::CreateInfo& info)
    : m_LogicalDevice(info.deviceContext.logicalDevice)
    , m_PhysicalDevice(info.deviceContext.physicalDevice)
    , m_Allocator(info.deviceContext.allocator)
    , m_PhysicalDeviceProps(info.deviceContext.physicalDeviceProperties)
    , m_GraphicsQueue(info.deviceContext.queueInfo.graphicsQueue)

{
	ASSERT(m_PhysicalDevice.getFormatProperties(vk::Format::eR8G8B8A8Srgb).optimalTilingFeatures &
	           vk::FormatFeatureFlagBits::eSampledImageFilterLinear,
	       "Texture image format(eR8G8B8A8Srgb) does not support linear blitting");

	vk::CommandPoolCreateInfo commandPoolCreateInfo {
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // flags
		info.deviceContext.queueInfo.graphicsQueueIndex,    // queueFamilyIndex
	};
	m_UploadContext.cmdPool = m_LogicalDevice.createCommandPool(commandPoolCreateInfo, nullptr);

	vk::CommandBufferAllocateInfo uploadContextCmdBufferAllocInfo {
		m_UploadContext.cmdPool,
		vk::CommandBufferLevel::ePrimary,
		1u,
	};
	m_UploadContext.cmdBuffer = m_LogicalDevice.allocateCommandBuffers(uploadContextCmdBufferAllocInfo)[0];

	m_UploadContext.fence = m_LogicalDevice.createFence({});
}

Texture* TextureSystem::CreateFromData(const Texture::CreateInfoData& info)
{
	m_Textures[HashStr(info.name.c_str())] = {
		.descriptorInfo = {},
		.width          = static_cast<uint32_t>(info.width),
		.height         = static_cast<uint32_t>(info.height),
		.channels       = 4ul,
		.mipLevels      = static_cast<uint32_t>(std::floor(std::log2(std::max(info.width, info.height))) + 1),
		.size           = info.size,
		.image          = {},
		.imageView      = {},
		.sampler        = {},
	};

	Texture& texture = m_Textures[HashStr(info.name.c_str())];

	/////////////////////////////////////////////////////////////////////////////////
	// Create the image and staging buffer
	{
		// Create vulkan image
		vk::ImageCreateInfo imageCreateInfo {
			{},                        // flags
			vk::ImageType::e2D,        // imageType
			vk::Format::eR8G8B8A8Srgb, // format

			/* extent */
			vk::Extent3D {
			    texture.width,  // width
			    texture.height, // height
			    1u,             // depth
			},
			texture.mipLevels,                                                                                              // mipLevels
			1u,                                                                                                             // arrayLayers
			vk::SampleCountFlagBits::e1,                                                                                    // samples
			vk::ImageTiling::eOptimal,                                                                                      // tiling
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled, // usage
			vk::SharingMode::eExclusive,                                                                                    // sharingMode
			0u,                                                                                                             // queueFamilyIndexCount
			nullptr,                                                                                                        // queueFamilyIndices
			vk::ImageLayout::eUndefined,                                                                                    // initialLayout
		};

		vma::AllocationCreateInfo imageAllocInfo({},
		                                         vma::MemoryUsage::eGpuOnly,
		                                         vk::MemoryPropertyFlagBits::eDeviceLocal);
		texture.image = m_Allocator.createImage(imageCreateInfo, imageAllocInfo);

		// Create staging buffer
		vk::BufferCreateInfo stagingBufferCrateInfo {
			{},                                    // flags
			info.size,                             // size
			vk::BufferUsageFlagBits::eTransferSrc, // usage
			vk::SharingMode::eExclusive,           // sharingMode
		};

		AllocatedBuffer stagingBuffer;
		vma::AllocationCreateInfo bufferAllocInfo({}, vma::MemoryUsage::eCpuOnly, { vk::MemoryPropertyFlagBits::eHostCached });
		stagingBuffer = m_Allocator.createBuffer(stagingBufferCrateInfo, bufferAllocInfo);

		// Copy data to staging buffer
		memcpy(m_Allocator.mapMemory(stagingBuffer), info.pixels, info.size);
		m_Allocator.unmapMemory(stagingBuffer);

		ImmediateSubmit([&](vk::CommandBuffer cmd) {
			TransitionLayout(texture, cmd, 0u, texture.mipLevels, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

			vk::BufferImageCopy bufferImageCopy {
				0u, // bufferOffset
				0u, // bufferRowLength
				0u, // bufferImageHeight

				/* imageSubresource */
				vk::ImageSubresourceLayers {
				    vk::ImageAspectFlagBits::eColor, // aspectMask
				    0u,                              // mipLevel
				    0u,                              // baseArrayLayer
				    1u,                              // layerCount
				},
				vk::Offset3D { 0, 0, 0 },                           // imageOffset
				vk::Extent3D { texture.width, texture.height, 1u }, // imageExtent
			};

			cmd.copyBufferToImage(stagingBuffer, texture.image, vk::ImageLayout::eTransferDstOptimal, 1u, &bufferImageCopy);

			/////////////////////////////////////////////////////////////////////////////////
			/// Create texture mipmaps
			vk::ImageMemoryBarrier barrier {
				{},                          // srcAccessmask
				{},                          // dstAccessMask
				vk::ImageLayout::eUndefined, // oldLayout
				vk::ImageLayout::eUndefined, // newLayout
				VK_QUEUE_FAMILY_IGNORED,     // srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,     // dstQueueFamilyIndex
				texture.image,               // image

				vk::ImageSubresourceRange {
				    vk::ImageAspectFlagBits::eColor, // aspectMask
				    0u,                              // baseMipLevel
				    1u,                              // levelCount
				    0u,                              // baseArrayLayer
				    1u,                              // layerCount
				},
			};

			int32_t mipWidth  = info.width;
			int32_t mipHeight = info.height;

			for (uint32_t i = 1; i < texture.mipLevels; i++)
			{
				TransitionLayout(texture, cmd, i - 1u, 1u, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal);
				BlitImage(cmd, texture.image, i, mipWidth, mipHeight);
				TransitionLayout(texture, cmd, i - 1u, 1u, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
			}

			TransitionLayout(texture, cmd, texture.mipLevels - 1ul, 1u, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		});

		// Copy buffer to image is executed, delete the staging buffer
		m_Allocator.destroyBuffer(stagingBuffer, stagingBuffer);

		/////////////////////////////////////////////////////////////////////////////////
		// Create image views
		{
			vk::ImageViewCreateInfo imageViewCreateInfo {
				{},                        // flags
				texture.image,             // image
				vk::ImageViewType::e2D,    // viewType
				vk::Format::eR8G8B8A8Srgb, // format

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
				    texture.mipLevels,               // levelCount
				    0u,                              // baseArrayLayer
				    1u,                              // layerCount
				},
			};

			texture.imageView = m_LogicalDevice.createImageView(imageViewCreateInfo, nullptr);
		}
		/////////////////////////////////////////////////////////////////////////////////
		// Create image samplers
		{
			vk::SamplerCreateInfo samplerCreateInfo {
				{},
				vk::Filter::eLinear,             // magFilter
				vk::Filter::eLinear,             //minFilter
				vk::SamplerMipmapMode::eLinear,  // mipmapMode
				vk::SamplerAddressMode::eRepeat, // addressModeU
				vk::SamplerAddressMode::eRepeat, // addressModeV
				vk::SamplerAddressMode::eRepeat, // addressModeW
				0.0f,                            // mipLodBias

				// #TODO ANISOTROPY
				false, // anisotropyEnable
				{},    // maxAnisotropy

				VK_FALSE,                              // compareEnable
				vk::CompareOp::eAlways,                // compareOp
				0.0f,                                  // minLod
				static_cast<float>(texture.mipLevels), // maxLod
				vk::BorderColor::eIntOpaqueBlack,      // borderColor
				VK_FALSE,                              // unnormalizedCoordinates
			};

			/// @todo Should we separate samplers and textures?
			texture.sampler = m_LogicalDevice.createSampler(samplerCreateInfo, nullptr);
		}

		texture.descriptorInfo = {
			texture.sampler,                         // sampler
			texture.imageView,                       // imageView
			vk::ImageLayout::eShaderReadOnlyOptimal, // imageLayout
		};
	}

	return &m_Textures[HashStr(info.name.c_str())];
}

Texture* TextureSystem::CreateFromGLTF(const Texture::CreateInfoGLTF& info)
{
	return CreateFromData({
	    .name   = info.image->uri,
	    .pixels = &info.image->image[0],
	    .width  = info.image->width,
	    .height = info.image->height,
	    .size   = info.image->image.size(),
	});
}

void TextureSystem::BlitImage(vk::CommandBuffer cmd, AllocatedImage image, uint32_t mipIndex, int32_t& mipWidth, int32_t& mipHeight)
{
	vk::ImageBlit blit {
		/* srcSubresource */
		vk::ImageSubresourceLayers {
		    vk::ImageAspectFlagBits::eColor, // aspectMask
		    mipIndex - 1u,                   // mipLevel
		    0u,                              // baseArrayLayer
		    1u,                              // layerCount
		},

		/* srcOffsets */
		{
		    vk::Offset3D { 0, 0, 0 },
		    vk::Offset3D { mipWidth, mipHeight, 1 },
		},

		/* dstSubresource */
		vk::ImageSubresourceLayers {
		    vk::ImageAspectFlagBits::eColor, // aspectMask
		    mipIndex,                        // mipLevel
		    0u,                              // baseArrayLayer
		    1u,                              // layerCount
		},

		/* dstOffsets */
		{
		    vk::Offset3D { 0, 0, 0 },
		    vk::Offset3D { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 },
		},
	};
	cmd.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
	              image, vk::ImageLayout::eTransferDstOptimal,
	              1u, &blit, vk::Filter::eLinear);
	if (mipWidth > 1)
		mipWidth /= 2;

	if (mipHeight > 1)
		mipHeight /= 2;
}

TextureSystem::~TextureSystem()
{
	for (auto& [key, value] : m_Textures)
	{
		m_LogicalDevice.destroySampler(value.sampler, nullptr);
		m_LogicalDevice.destroyImageView(value.imageView, nullptr);
		m_Allocator.destroyImage(value.image, value.image);
	}
}

void TextureSystem::TransitionLayout(Texture& texture, vk::CommandBuffer cmdBuffer, uint32_t baseMipLevel, uint32_t levelCount, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
	// Memory barrier
	vk::ImageMemoryBarrier imageMemBarrier {
		{},                      // srcAccessMask
		{},                      // dstAccessMask
		oldLayout,               // oldLayout
		newLayout,               // newLayout
		VK_QUEUE_FAMILY_IGNORED, // srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED, // dstQueueFamilyIndex
		texture.image,           // image

		/* subresourceRange */
		vk::ImageSubresourceRange {
		    vk::ImageAspectFlagBits::eColor, // aspectMask
		    baseMipLevel,                              // baseMipLevel
		    levelCount,               // levelCount
		    0u,                              // baseArrayLayer
		    1u,                              // layerCount
		},
	};
	/* Specify the source & destination stages and access masks... */
	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;

	// Undefined -> TRANSFER DST
	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		imageMemBarrier.srcAccessMask = {};
		imageMemBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}

	// TRANSFER DST -> TRANSFER SRC
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eTransferSrcOptimal)
	{
		imageMemBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		imageMemBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}

	// TRANSFER SRC -> SHADER READ
	else if (oldLayout == vk::ImageLayout::eTransferSrcOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		imageMemBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		imageMemBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	//
	// TRANSFER DST -> SHADER READ
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		imageMemBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		imageMemBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}

	else
	{
		/// @todo: Stringifier
		LOG(err, "Texture transition layout to/from unexpected layout(s) \n {} -> {}", (int32_t)oldLayout, (int32_t)newLayout);
	}

	// Execute pipeline barrier
	cmdBuffer.pipelineBarrier(
	    srcStage, dstStage,
	    {},
	    0, nullptr,
	    0, nullptr,
	    1, &imageMemBarrier);
}

void TextureSystem::ImmediateSubmit(std::function<void(vk::CommandBuffer)>&& function)
{
	vk::CommandBuffer cmd = m_UploadContext.cmdBuffer;

	vk::CommandBufferBeginInfo beginInfo {
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	};

	cmd.begin(beginInfo);

	function(cmd);

	cmd.end();

	vk::SubmitInfo submitInfo { 0u, {}, {}, 1u, &cmd, 0u, {}, {} };

	m_GraphicsQueue.submit(submitInfo, m_UploadContext.fence);
	VKC(m_LogicalDevice.waitForFences(m_UploadContext.fence, true, UINT_MAX));

	m_LogicalDevice.resetFences(m_UploadContext.fence);
	m_LogicalDevice.resetCommandPool(m_UploadContext.cmdPool, {});
}
