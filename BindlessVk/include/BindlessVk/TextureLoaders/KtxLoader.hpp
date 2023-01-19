#pragma once

#include "BindlessVk/Buffer.hpp"
#include "BindlessVk/Common/Common.hpp"
#include "BindlessVk/Device.hpp"
#include "BindlessVk/Texture.hpp"

#include <AssetParser.hpp>
#include <TextureAsset.hpp>
#include <ktx.h>
#include <ktxvulkan.h>

namespace BINDLESSVK_NAMESPACE {

/// @todo Should we separate samplers and textures?
class KtxLoader
{
public:
	KtxLoader(Device* device, Buffer* staging_buffer);
	KtxLoader() = default;
	~KtxLoader() = default;

	Texture load(
	    c_str name,
	    c_str path,
	    Texture::Type type,
	    vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal
	);

private:
	void load_ktx_texture(const str& path);
	void destroy_ktx_texture();

	void load_texture_data();
	void write_texture_data_to_gpu();

	void create_image();
	void create_image_view();
	void create_sampler();

	vec<vk::BufferImageCopy> create_texture_face_buffer_copies();

private:
	Device* device = {};

	Texture texture = {};
	ktxTexture* ktx_texture = {};
	vk::ImageLayout final_layout;

	Buffer* staging_buffer = {};
};

} // namespace BINDLESSVK_NAMESPACE
