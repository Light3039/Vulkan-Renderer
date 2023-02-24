#pragma once

#include "BindlessVk/Buffer.hpp"
#include "BindlessVk/Common/Common.hpp"
#include "BindlessVk/Context/VkContext.hpp"
#include "BindlessVk/Texture/Texture.hpp"

#include <AssetParser.hpp>
#include <TextureAsset.hpp>
#include <ktx.h>
#include <ktxvulkan.h>

static_assert(KTX_SUCCESS == false, "KTX_SUCCESS was supposed to be 0 (false), but it isn't");

namespace BINDLESSVK_NAMESPACE {

/** @warn KtxLoader assumes cubemap type for now...  */
class KtxLoader
{
public:
	KtxLoader(VkContext const *vk_context, Buffer *staging_buffer);
	KtxLoader() = default;
	~KtxLoader() = default;

	Texture load(
	    c_str name,
	    c_str path,
	    Texture::Type type,
	    vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
	);

private:
	void load_ktx_texture(str const &path);
	void destroy_ktx_texture();

	void stage_texture_data();
	void write_texture_data_to_gpu(vk::ImageLayout final_layout);

	void create_image();
	void create_image_view();
	void create_sampler();

	auto create_texture_face_buffer_copies() -> vec<vk::BufferImageCopy>;

private:
	VkContext const *const vk_context = {};
	Buffer *const staging_buffer = {};

	Texture texture = {};
	ktxTexture *ktx_texture = {};
};

} // namespace BINDLESSVK_NAMESPACE
