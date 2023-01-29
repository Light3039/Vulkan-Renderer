#pragma once

#include "BindlessVk/Common/Common.hpp"
#include "BindlessVk/Model.hpp"
#include "BindlessVk/TextureLoader.hpp"
#include "BindlessVk/VkContext.hpp"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace BINDLESSVK_NAMESPACE {

/**
 * @brief Loads model files like gltf, fbx, etc.
 *
 * @todo feat: support fbx
 * @todo feat: support obj
 *
 * @todo refactor: bindlessvk shouldn't be responsible for STORING textures
 */
class ModelLoader
{
public:
	/**
	 * @brief Main constructor
	 * @param vk_context the vulkan context
	 * @param texture_system the bindlessvk texture system, ModelLoader may load textures
	 */
	ModelLoader(VkContext const *vk_context, TextureLoader const *texture_system);

	/** @brief Default constructor */
	ModelLoader() = default;

	/** @brief Default destructor */
	~ModelLoader() = default;

	/**
	 * @brief Loads a model from a gltf file
	 * @param name debug name attached to vulkan objects for debugging tools like renderdoc
	 * @param file_path path to the gltf model file
	 */
	auto load_from_gltf_ascii(
	    c_str debug_name,
	    c_str file_path,
	    Buffer *staging_vertex_buffer,
	    Buffer *staging_index_buffer,
	    Buffer *staging_image_buffer
	) const -> Model;

	/** @todo Implement */
	auto load_from_gltf_binary() -> Model = delete;

	/** @todo Implement */
	auto load_from_fbx() -> Model = delete;

	/** @todo Implement */
	auto load_from_obj() -> Model = delete;

private:
	VkContext const *vk_context = {};
	TextureLoader const *texture_loader = {};
};

} // namespace BINDLESSVK_NAMESPACE
