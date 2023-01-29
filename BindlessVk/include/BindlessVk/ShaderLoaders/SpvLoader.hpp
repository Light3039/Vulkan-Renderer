#pragma once

#include "BindlessVk/Common/Common.hpp"
#include "BindlessVk/Shader.hpp"
#include "BindlessVk/VkContext.hpp"

#include <spirv_reflect.h>

static_assert(
    SPV_REFLECT_RESULT_SUCCESS == false,
    "SPV_REFLECT_RESULT_SUCCESS was suppoed to be 0 (false), but it isn't"
);

namespace BINDLESSVK_NAMESPACE {

class SpvLoader
{
public:
	SpvLoader(VkContext const *vk_context);

	Shader load(c_str path);

private:
	void load_code(c_str path);
	void reflect_code();
	void create_vulkan_shader_module();

	void reflect_descriptor_sets();
	void reflect_shader_stage();

	auto reflect_descriptor_set_bindings(SpvReflectDescriptorSet const *spv_set)
	    -> vec<vk::DescriptorSetLayoutBinding>;

	auto extract_descriptor_set_binding(SpvReflectDescriptorBinding const *binding)
	    -> vk::DescriptorSetLayoutBinding;

private:
	VkContext const *const vk_context = {};
	Shader shader = {};
	vec<u32> code = {};
	SpvReflectShaderModule reflection = {};
};

} // namespace BINDLESSVK_NAMESPACE
