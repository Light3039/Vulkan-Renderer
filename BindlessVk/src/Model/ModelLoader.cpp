#include "BindlessVk/Model/ModelLoader.hpp"

#include "BindlessVk/Buffer.hpp"
#include "BindlessVk/Model/Loaders/GltfLoader.hpp"

#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>

namespace BINDLESSVK_NAMESPACE {

ModelLoader::ModelLoader(ref<VkContext const> const vk_context)
    : vk_context(vk_context)
    , texture_loader(vk_context)
{
}

auto ModelLoader::load_from_gltf_ascii(
    str_view file_path,
    Buffer *const staging_vertex_buffer,
    Buffer *const staging_index_buffer,
    Buffer *const staging_image_buffer,
    str_view debug_name /* = default_debug_name */
) const -> Model
{
	GltfLoader loader(
	    vk_context.get(),
	    &texture_loader,
	    staging_vertex_buffer,
	    staging_index_buffer, //
	    staging_image_buffer
	);

	return loader.load_from_ascii(file_path, debug_name);
}

} // namespace BINDLESSVK_NAMESPACE
