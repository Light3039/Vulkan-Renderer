#pragma once

#include "BindlessVk/Common.hpp"
#include "BindlessVk/Device.hpp"
#include "BindlessVk/Texture.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace tinygltf {
struct Node;
struct Model;
struct Mesh;
struct Primitive;
} // namespace tinygltf

namespace BINDLESSVK_NAMESPACE {

namespace VertexTypes {

struct Model
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;
	glm::vec3 color;

	static constexpr std::array<vk::VertexInputBindingDescription, 1> get_bindings()
	{
		return {
			vk::VertexInputBindingDescription {
			    0u,                                                // binding
			    static_cast<uint32_t>(sizeof(VertexTypes::Model)), // stride
			    vk::VertexInputRate::eVertex,                      // inputRate
			},
		};
	}

	static constexpr std::array<vk::VertexInputAttributeDescription, 5> get_attributes()
	{
		return {
			vk::VertexInputAttributeDescription {
			    0u,                                     // location
			    0u,                                     // binding
			    vk::Format::eR32G32B32Sfloat,           // format
			    offsetof(VertexTypes::Model, position), // offset
			},
			vk::VertexInputAttributeDescription {
			    1u,                                   // location
			    0u,                                   // binding
			    vk::Format::eR32G32B32Sfloat,         // format
			    offsetof(VertexTypes::Model, normal), // offset
			},
			vk::VertexInputAttributeDescription {
			    2u,                                    // location
			    0u,                                    // binding
			    vk::Format::eR32G32B32Sfloat,          // format
			    offsetof(VertexTypes::Model, tangent), // offset
			},
			vk::VertexInputAttributeDescription {
			    3u,                               // location
			    0u,                               // binding
			    vk::Format::eR32G32Sfloat,        // format
			    offsetof(VertexTypes::Model, uv), // offset
			},
			vk::VertexInputAttributeDescription {
			    4u,                                  // location
			    0u,                                  // binding
			    vk::Format::eR32G32B32Sfloat,        // format
			    offsetof(VertexTypes::Model, color), // offset
			},


		};
		offsetof(VertexTypes::Model, uv);
	}

	static vk::PipelineVertexInputStateCreateInfo get_vertex_input_state()
	{
		static const auto bindings = get_bindings();
		static const auto attributes = get_attributes();

		return vk::PipelineVertexInputStateCreateInfo {
			{},
			static_cast<uint32_t>(bindings.size()),
			bindings.data(),

			static_cast<uint32_t>(attributes.size()),
			attributes.data(),
		};
	}
};

} // namespace VertexTypes

struct Model
{
public:
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec3 tangent;
		glm::vec2 uv;
		glm::vec3 color; // colour matey
	};

	struct Primitive
	{
		uint32_t first_index;
		uint32_t index_count;
		int32_t material_index;
	};

	struct Node
	{
		Node(Node* parent): parent(parent)
		{
		}

		~Node()
		{
			for (auto* node : children)
				delete node;
		}

		Node* parent;
		std::vector<Node*> children;

		std::vector<Primitive> mesh;
		glm::mat4 transform;
	};

	struct MaterialParameters
	{
		glm::vec3 albedo_factor = glm::vec4(1.0f);
		glm::vec3 diffuse_factor = glm::vec4(1.0f);
		glm::vec3 specular_factor = glm::vec4(1.0f);

		int32_t albedo_texture_index;
		int32_t normal_texture_index;
		int32_t metallic_roughness_texture_index;
	};

	struct Texture
	{
		int32_t image_index;
	};

	/// @todo: Support textures with different samplers
	std::vector<BINDLESSVK_NAMESPACE::Texture*> textures;
	std::vector<MaterialParameters> material_parameters;
	std::vector<Node*> nodes;
	class StagingBuffer* vertex_buffer;
	class StagingBuffer* index_buffer;
};

class ModelSystem
{
public:
	ModelSystem() = default;

	/** Initializes the model system
	 * @param device the bindlessvk device
	 */
	void init(Device* device, TextureSystem* texture_system);

	/** Destroys the model system */
	void reset();

	/** Loads a model from a gltf file
	 * @param texture_system the bindlessvk texture system
	 * @param name name of the model
	 * @param gltf_path path to the gltf model file
	 * @todo tidy this mess of passing texture system around??
	 */
	void load_gltf(const char* name, const char* gltf_path);

	/** @return the model named @p name */
	inline Model* get_model(const char* name)
	{
		return &models[HashStr(name)];
	}

private:
	tinygltf::Model load_gltf_file(const char* gltf_path);
	void load_mesh(const tinygltf::Model& gltf_model, Model& model);
	void load_textures(const tinygltf::Model& gltf_model, Model& model);
	void load_material_parameters(const tinygltf::Model& gltf_model, Model& model);
	void create_model_gpu_buffers(const char* name, Model& model);

	Model::Node* load_node(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Node& gltf_node,
	    Model& model,
	    Model::Node* parent_node
	);

	void load_mesh_primitives(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Mesh& gltf_mesh,
	    Model::Node* node
	);

	void load_mesh_primitive_vertices(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Primitive& gltf_primitive
	);

	u32 load_mesh_primitive_indices(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Primitive& gltf_primitive
	);

	const float* get_mesh_primitive_position_buffer(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Primitive& gltf_primitive
	);

	const float* get_mesh_primitive_normal_buffer(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Primitive& gltf_primitive
	);

	const float* get_mesh_primitive_tangent_buffer(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Primitive& gltf_primitive
	);

	const float* get_mesh_primitive_uv_buffer(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Primitive& gltf_primitive
	);

	size_t get_mesh_primitive_vertex_count(
	    const tinygltf::Model& gltf_model,
	    const tinygltf::Primitive& gltf_primitive
	);

	void set_initial_node_transform(const tinygltf::Node& gltf_node, Model::Node* node);

	void free_staging_data();

	bool node_has_any_children(const tinygltf::Node& gltf_node);
	bool node_has_any_mesh(const tinygltf::Node& gltf_node);


private:
	Device* device;

	TextureSystem* texture_system;

	vk::CommandPool command_pool;
	std::unordered_map<uint64_t, Model> models;

	std::vector<Model::Vertex> staging_vertex_buffer;
	std::vector<uint32_t> staging_index_buffer;
};

} // namespace BINDLESSVK_NAMESPACE
