#pragma once

#include "BindlessVk/Allocators/Descriptors/DescriptorAllocator.hpp"
#include "BindlessVk/Shader/DescriptorSet.hpp"
#include "BindlessVk/Common/Common.hpp"
#include "BindlessVk/Context/VkContext.hpp"
#include "BindlessVk/Shader/Shader.hpp"

namespace BINDLESSVK_NAMESPACE {

class Material
{
public:
	struct Parameters
	{
		arr<f32, 4> albedo;
		arr<f32, 4> emissive;
		arr<f32, 4> diffuse;
		arr<f32, 4> specular;
		f32 metallic;
		f32 roughness;
	};

public:
	/** Default constructor */
	Material() = default;

	/** Argumented constructor
	 *
	 * @param
	 */
	Material(
	    DescriptorAllocator *descriptor_allocator,
	    ShaderPipeline *shader_pipeline,
	    vk::DescriptorPool descriptor_pool
	);

	/** Move constructor */
	Material(Material &&other);

	/** Move assignment operator */
	Material &operator=(Material &&other);

	/** Deleted copy constructor */
	Material(Material const &) = delete;

	/** Deleted copy assignment operator */
	Material &operator=(Material const &) = delete;

	/** Destructor */
	~Material();

	/** Trivial accessor for effect */
	auto *get_shader_pipeline() const
	{
		return shader_pipeline;
	}

	/** Accessor for the descriptor set's underlying descriptor set */
	auto get_descriptor_set() const
	{
		return descriptor_set.vk();
	}

private:
	DescriptorAllocator *descriptor_allocator;
	ShaderPipeline *shader_pipeline = {};
	Parameters parameters = {};
	DescriptorSet descriptor_set = {};
};

} // namespace BINDLESSVK_NAMESPACE
