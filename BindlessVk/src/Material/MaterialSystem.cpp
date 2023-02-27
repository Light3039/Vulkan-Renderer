#include "BindlessVk/Material/MaterialSystem.hpp"

#include <ranges>

namespace BINDLESSVK_NAMESPACE {

ShaderEffect::ShaderEffect(
    VkContext *const vk_context,
    vec<Shader *> const &shaders,
    ShaderEffect::Configuration const configuration,
    c_str const debug_name /* = "" */
)
    : vk_context(vk_context)
    , debug_name(debug_name)
{
	create_descriptor_sets_layout(shaders);

	auto const device = vk_context->get_device();
	auto const surface = vk_context->get_surface();
	auto const pipeline_shader_stage_create_infos = create_pipeline_shader_stage_infos(shaders);
	auto const surface_color_format = surface.get_color_format();

	pipeline_layout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo {
	    {},
	    static_cast<u32>(descriptor_sets_layout.size()),
	    descriptor_sets_layout.data(),
	});
	vk_context->set_object_name(
	    pipeline_layout,
	    fmt::format("{}_pipeline_layout", this->debug_name)
	);

	const auto pipeline_rendering_info = vk::PipelineRenderingCreateInfo {
		{},
		surface_color_format,
		vk_context->get_depth_format(),
		{},
	};

	pipeline = create_graphics_pipeline(
	    pipeline_shader_stage_create_infos,
	    pipeline_rendering_info,
	    configuration
	);

	vk_context->set_object_name(pipeline, fmt::format("{}_pipeline", this->debug_name));
}

ShaderEffect::ShaderEffect(ShaderEffect &&effect)
{
	*this = std::move(effect);
}

ShaderEffect &ShaderEffect::operator=(ShaderEffect &&effect)
{
	this->vk_context = effect.vk_context;
	this->pipeline = effect.pipeline;
	this->pipeline_layout = effect.pipeline_layout;
	this->descriptor_sets_layout = effect.descriptor_sets_layout;

	effect.vk_context = {};

	return *this;
}

ShaderEffect::~ShaderEffect()
{
	if (vk_context)
	{
		const auto device = vk_context->get_device();

		device.destroyPipeline(pipeline);
		device.destroyPipelineLayout(pipeline_layout);
		device.destroyDescriptorSetLayout(descriptor_sets_layout[0]);
		device.destroyDescriptorSetLayout(descriptor_sets_layout[1]);
	}
}

void ShaderEffect::create_descriptor_sets_layout(vec<Shader *> const &shaders)
{
	const auto device = vk_context->get_device();
	const auto sets_bindings = combine_descriptor_sets_bindings(shaders);

	for (u32 i = 0u; const auto &set_bindings : sets_bindings)
	{
		descriptor_sets_layout[i] =
		    device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo {
		        {},
		        static_cast<u32>(set_bindings.size()),
		        set_bindings.data(),
		    });
		vk_context->set_object_name(
		    descriptor_sets_layout[i],
		    fmt::format("{}_descriptor_set_layout_{}", debug_name, i)
		);

		i++;
	}
}

arr<vec<vk::DescriptorSetLayoutBinding>, 2> ShaderEffect::combine_descriptor_sets_bindings(
    vec<Shader *> const &shaders
)
{
	auto combined_bindings = arr<vec<vk::DescriptorSetLayoutBinding>, 2> {};

	for (Shader *const shader : shaders)
	{
		for (u32 i = 0; const auto &descriptor_set_bindings : shader->descriptor_sets_bindings)
		{
			for (const auto &descriptor_set_binding : descriptor_set_bindings)
			{
				const u32 set_index = i;
				const u32 binding_index = descriptor_set_binding.binding;

				if (combined_bindings[i].size() <= binding_index)
					combined_bindings[i].resize(binding_index + 1);

				combined_bindings[i][binding_index] = descriptor_set_binding;
			}

			i++;
		}
	}

	return combined_bindings;
}

vec<vk::PipelineShaderStageCreateInfo> ShaderEffect::create_pipeline_shader_stage_infos(
    vec<Shader *> shaders
)
{
	auto pipeline_shader_stage_infos = vec<vk::PipelineShaderStageCreateInfo> {};
	pipeline_shader_stage_infos.resize(shaders.size());

	for (u32 i = 0; const auto &shader : shaders)
	{
		pipeline_shader_stage_infos[i++] = {
			{},
			shader->stage,
			shader->module,
			"main",
		};
	}

	return pipeline_shader_stage_infos;
}

vk::Pipeline ShaderEffect::create_graphics_pipeline(
    vec<vk::PipelineShaderStageCreateInfo> shader_stage_create_infos,
    vk::PipelineRenderingCreateInfoKHR rendering_info,
    ShaderEffect::Configuration configuration
)
{
	const auto device = vk_context->get_device();

	const auto color_blend_state = vk::PipelineColorBlendStateCreateInfo {
		{},
		{},
		{},
		static_cast<u32>(configuration.color_blend_attachments.size()),
		configuration.color_blend_attachments.data()
	};

	const auto dynamic_state = vk::PipelineDynamicStateCreateInfo {
		{},
		static_cast<u32>(configuration.dynamic_states.size()),
		configuration.dynamic_states.data(),
	};

	const auto graphics_pipeline_info = vk::GraphicsPipelineCreateInfo {
		{},
		static_cast<u32>(shader_stage_create_infos.size()),
		shader_stage_create_infos.data(),
		&configuration.vertex_input_state,
		&configuration.input_assembly_state,
		&configuration.tesselation_state,
		&configuration.viewport_state,
		&configuration.rasterization_state,
		&configuration.multisample_state,
		&configuration.depth_stencil_state,
		&color_blend_state,
		&dynamic_state,
		pipeline_layout,
		{},
		{},
		{},
		{},
		&rendering_info,
	};

	const auto pipeline = device.createGraphicsPipeline({}, graphics_pipeline_info);
	assert_false(pipeline.result);

	return pipeline.value;
}

Material::Material(
    VkContext *const vk_context,
    ShaderEffect *const effect,
    vk::DescriptorPool const descriptor_pool
)
    : effect(effect)
{
	auto *const descriptor_allocator = vk_context->get_descriptor_allocator();
	vk::DescriptorSetAllocateInfo allocate_info {
		descriptor_pool,
		1,
		&effect->get_descriptor_set_layouts().back(),
	};

	descriptor_allocator->allocate_descriptor_set(effect->get_descriptor_set_layouts().back());
	const auto device = vk_context->get_device();
	assert_false(device.allocateDescriptorSets(&allocate_info, &descriptor_set));
}


} // namespace BINDLESSVK_NAMESPACE
