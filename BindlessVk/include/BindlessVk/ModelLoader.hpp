#pragma once

#include "BindlessVk/Common/Common.hpp"
#include "BindlessVk/Device.hpp"
#include "BindlessVk/Model.hpp"
#include "BindlessVk/Texture.hpp"

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
	 * @param device the bindlessvk device
	 * @param texture_system the bindlessvk texture system, ModelLoader may load textures
	 */
	ModelLoader(Device* device, TextureSystem* texture_system);

	/** @brief Default constructor */
	ModelLoader() = default;

	/** @brief Default destructor */
	~ModelLoader() = default;

	/**
	 * @brief Loads a model from a gltf file
	 * @param name debug name attached to vulkan objects for debugging tools like renderdoc
	 * @param gltf_path path to the gltf model file
	 */
	Model load_from_gltf_ascii(
	    const char* debug_name,
	    const char* file_path,
	    Buffer* staging_vertex_buffer,
	    Buffer* staging_index_buffer
	);

private:
	Device* device;
	TextureSystem* texture_system;
};

} // namespace BINDLESSVK_NAMESPACE
