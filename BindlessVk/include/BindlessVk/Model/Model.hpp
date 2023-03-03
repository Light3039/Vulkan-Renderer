#pragma once

#include "BindlessVk/Common/Common.hpp"
#include "BindlessVk/Texture/Texture.hpp"

#include <vulkan/vulkan.hpp>

namespace BINDLESSVK_NAMESPACE {

class Model
{
public:
	friend class ModelLoader;
	friend class GltfLoader;

public:
	struct Vertex
	{
		vec3 position;
		vec3 normal;
		vec3 tangent;
		vec2 uv;
		vec3 color;

		auto static get_attributes() -> arr<vk::VertexInputAttributeDescription, 5>;
		auto static get_bindings() -> arr<vk::VertexInputBindingDescription, 1>;
		auto static get_vertex_input_state() -> vk::PipelineVertexInputStateCreateInfo;
	};

	struct Node
	{
		struct Primitive
		{
			u32 first_index;
			u32 index_count;
			i32 material_index;
		};

		Node(Node *parent): parent(parent)
		{
		}

		~Node()
		{
			for (auto *node : children)
				delete node;
		}

		Node *parent;
		vec<Node *> children;

		vec<Primitive> mesh;
		mat4f transform;
	};

	struct MaterialParameters
	{
		vec3 albedo = vec3(1.0f);
		vec3 diffuse = vec3(1.0f);
		vec3 specular = vec3(1.0f);

		i32 albedo_texture_index;
		i32 normal_texture_index;
		i32 metallic_roughness_texture_index;
	};

public:
	~Model();

	Model(Model &&);
	Model &operator=(Model &&);

	Model(const Model &) = delete;
	Model &operator=(const Model &) = delete;

	inline auto get_name() const
	{
		return str_view(debug_name);
	}

	inline auto &get_nodes() const
	{
		return nodes;
	}

	inline auto &get_textures() const
	{
		return textures;
	}

	inline auto &get_material_parameters() const
	{
		return material_parameters;
	}

	inline auto *get_vertex_buffer() const
	{
		return vertex_buffer;
	}

	inline auto *get_index_buffer() const
	{
		return index_buffer;
	}

private:
	Model() = default;

private:
	vec<Node *> nodes = {};
	vec<Texture> textures = {};
	vec<MaterialParameters> material_parameters = {};

	class Buffer *vertex_buffer = {};
	class Buffer *index_buffer = {};

	str debug_name = {};
};

} // namespace BINDLESSVK_NAMESPACE
