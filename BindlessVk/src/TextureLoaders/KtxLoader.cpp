#include "BindlessVk/TextureLoaders/KtxLoader.hpp"


namespace BINDLESSVK_NAMESPACE {

KtxLoader::KtxLoader(VkContext const *const vk_context, Buffer *const staging_buffer)
    : vk_context(vk_context)
    , staging_buffer(staging_buffer)
{
}

Texture KtxLoader::load(
    c_str const name,
    c_str const path,
    Texture::Type const type,
    vk::ImageLayout const final_layout /* = vk::ImageLayout::eShaderReadOnlyOptimal */
)
{
	texture = { name };

	load_ktx_texture(path);

	create_image();
	create_image_view();
	create_sampler();

	stage_texture_data();
	write_texture_data_to_gpu(final_layout);

	destroy_ktx_texture();

	return texture;
}

void KtxLoader::load_ktx_texture(str const &path)
{
	assert_false(
	    ktxTexture_CreateFromNamedFile(
	        path.c_str(),
	        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
	        &ktx_texture
	    ),
	    "Failed to load ktx file: \nname: {}\npath: {}",
	    texture.name,
	    path
	);

	texture.width = ktx_texture->baseWidth;
	texture.height = ktx_texture->baseHeight;
	texture.format = vk::Format::eB8G8R8A8Srgb;
	texture.mip_levels = ktx_texture->numLevels;
	texture.size = ktxTexture_GetSize(ktx_texture);
	texture.current_layout = vk::ImageLayout::eUndefined;
}

void KtxLoader::destroy_ktx_texture()
{
	ktxTexture_Destroy(ktx_texture);
}

void KtxLoader::create_image()
{
	auto const allocator = vk_context->get_allocator();

	texture.image = allocator.createImage(
	    vk::ImageCreateInfo {
	        vk::ImageCreateFlagBits::eCubeCompatible,
	        vk::ImageType::e2D,
	        texture.format,
	        vk::Extent3D {
	            texture.width,
	            texture.height,
	            1u,
	        },
	        texture.mip_levels,
	        6u,
	        vk::SampleCountFlagBits::e1,
	        vk::ImageTiling::eOptimal,
	        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
	        vk::SharingMode::eExclusive,
	        0u,
	        nullptr,
	        vk::ImageLayout::eUndefined,
	    },

	    vma::AllocationCreateInfo {
	        {},
	        vma::MemoryUsage::eGpuOnly,
	        vk::MemoryPropertyFlagBits::eDeviceLocal,
	    }
	);
}

void KtxLoader::create_image_view()
{
	auto const device = vk_context->get_device();

	texture.image_view = device.createImageView(vk::ImageViewCreateInfo {
	    {},
	    texture.image,
	    vk::ImageViewType::eCube,
	    texture.format,
	    vk::ComponentMapping {
	        vk::ComponentSwizzle::eIdentity,
	        vk::ComponentSwizzle::eIdentity,
	        vk::ComponentSwizzle::eIdentity,
	        vk::ComponentSwizzle::eIdentity,
	    },
	    vk::ImageSubresourceRange {
	        vk::ImageAspectFlagBits::eColor,
	        0u,
	        texture.mip_levels,
	        0u,
	        6u,
	    },
	});
	texture.descriptor_info.imageView = texture.image_view;
}

void KtxLoader::create_sampler()
{
	auto const device = vk_context->get_device();

	texture.sampler = device.createSampler(vk::SamplerCreateInfo {
	    {},
	    vk::Filter::eLinear,
	    vk::Filter::eLinear,
	    vk::SamplerMipmapMode::eLinear,
	    vk::SamplerAddressMode::eClampToEdge,
	    vk::SamplerAddressMode::eClampToEdge,
	    vk::SamplerAddressMode::eClampToEdge,
	    0.0f,
	    VK_FALSE, // @todo anisotropy
	    {},
	    VK_FALSE,
	    vk::CompareOp::eNever,
	    0.0f,
	    static_cast<float>(texture.mip_levels),
	    vk::BorderColor::eFloatOpaqueWhite,
	    VK_FALSE,
	});
	texture.descriptor_info.sampler = texture.sampler;
}

void KtxLoader::stage_texture_data()
{
	u8 const *const data = ktxTexture_GetData(ktx_texture);
	memcpy(staging_buffer->map_block(0), reinterpret_cast<const void *const>(data), texture.size);
	staging_buffer->unmap();
}

void KtxLoader::write_texture_data_to_gpu(vk::ImageLayout const final_layout)
{
	auto const buffer_copies = create_texture_face_buffer_copies();

	vk_context->immediate_submit([&](vk::CommandBuffer &&cmd) {
		texture.transition_layout(
		    vk_context,
		    cmd,
		    0u,
		    texture.mip_levels,
		    6u,
		    vk::ImageLayout::eTransferDstOptimal
		);

		cmd.copyBufferToImage(
		    *staging_buffer->get_buffer(),
		    texture.image,
		    vk::ImageLayout::eTransferDstOptimal,
		    static_cast<u32>(buffer_copies.size()),
		    buffer_copies.data()
		);

		texture.transition_layout(vk_context, cmd, 0u, texture.mip_levels, 6u, final_layout);
	});

	texture.descriptor_info.imageLayout = texture.current_layout;
}

vec<vk::BufferImageCopy> KtxLoader::create_texture_face_buffer_copies()
{
	auto buffer_copies = vec<vk::BufferImageCopy> {};

	for (u32 face = 0u; face < 6u; ++face)
	{
		for (u32 level = 0u; level < texture.mip_levels; ++level)
		{
			usize offset;
			assert_false(
			    ktxTexture_GetImageOffset(ktx_texture, level, 0u, face, &offset),
			    "Failed to get ktx image offset"
			);

			buffer_copies.emplace_back(vk::BufferImageCopy {
			    offset,
			    {},
			    {},
			    {
			        vk::ImageAspectFlagBits::eColor,
			        level,
			        face,
			        1u,
			    },
			    {},
			    {
			        texture.width >> level,
			        texture.height >> level,
			        1u,
			    },
			});
		}
	}

	return buffer_copies;
}

} // namespace BINDLESSVK_NAMESPACE
