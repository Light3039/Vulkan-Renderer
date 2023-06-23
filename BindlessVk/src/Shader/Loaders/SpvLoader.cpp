#include "BindlessVk/Shader/Loaders/SpvLoader.hpp"



namespace BINDLESSVK_NAMESPACE {

SpvLoader::SpvLoader(VkContext const *const vk_context): device(vk_context->get_device())
{
	ZoneScoped;
}

Shader SpvLoader::load(str_view const path)
{
	ZoneScoped;

	load_code(path);
	reflect_code();

	create_vulkan_shader_module();

	return shader;
}

void SpvLoader::load_code(str_view const path)
{
	ZoneScoped;

	auto file_stream = std::ifstream(path.data(), std::ios::ate);
	usize const file_size = file_stream.tellg();
	code.resize(file_size / sizeof(u32));

	file_stream.seekg(0);
	file_stream.read((char *)code.data(), file_size);
	file_stream.close();
}

void SpvLoader::reflect_code()
{
	ZoneScoped;

	assert_false(
	    spvReflectCreateShaderModule(code.size() * sizeof(u32), code.data(), &reflection),
	    "spvReflectCreateShderModule failed"
	);

	reflect_shader_stage();
	reflect_descriptor_sets();
}

void SpvLoader::create_vulkan_shader_module()
{
	ZoneScoped;

	shader.module = device->vk().createShaderModule(vk::ShaderModuleCreateInfo {
	    {},
	    code.size() * sizeof(u32),
	    code.data(),
	});
}

void SpvLoader::reflect_descriptor_sets()
{
	ZoneScoped;

	u32 descriptor_sets_count = 0;
	assert_false(
	    spvReflectEnumerateDescriptorSets(&reflection, &descriptor_sets_count, nullptr),
	    "spvReflectEnumerateDescriptorSets failed"
	);

	vec<SpvReflectDescriptorSet *> descriptor_sets_reflection(descriptor_sets_count);
	assert_false(
	    spvReflectEnumerateDescriptorSets(
	        &reflection,
	        &descriptor_sets_count,
	        descriptor_sets_reflection.data()
	    ),
	    "spvReflectEnumerateDescriptorSets failed"
	);

	// shader uses (set = 2) descriptor set, which is the per-shader set slot
	// set = 1 -> per pass
	// set = 0 -> per graph(frame)
	if (descriptor_sets_reflection.size() >= 2)
	{
		auto const *const spv_set = descriptor_sets_reflection[2];
		shader.descriptor_set_bindings = reflect_descriptor_set_bindings(spv_set);
	}
}

auto SpvLoader::reflect_descriptor_set_bindings(SpvReflectDescriptorSet const *const spv_set)
    -> vec<vk::DescriptorSetLayoutBinding>
{
	ZoneScoped;

	vec<vk::DescriptorSetLayoutBinding> bindings = {};

	for (u32 i_binding = 0; i_binding < spv_set->binding_count; ++i_binding)
	{
		auto const &spv_binding = *spv_set->bindings[i_binding];
		if (bindings.size() < spv_binding.binding + 1u)
			bindings.resize(spv_binding.binding + 1u);

		const u32 set_index = spv_binding.set;
		const u32 binding_index = spv_binding.binding;

		bindings[binding_index] = extract_descriptor_set_binding(spv_set->bindings[i_binding]);
	}

	return bindings;
}

auto SpvLoader::extract_descriptor_set_binding(SpvReflectDescriptorBinding const *const binding)
    -> vk::DescriptorSetLayoutBinding
{
	ZoneScoped;

	vk::DescriptorSetLayoutBinding set_binding;
	set_binding = binding->binding;

	set_binding.descriptorType = static_cast<vk::DescriptorType>(binding->descriptor_type);

	set_binding.stageFlags = shader.stage;

	set_binding.descriptorCount = 1u;

	for (u32 i_dim = 0; i_dim < binding->array.dims_count; ++i_dim)
		set_binding.descriptorCount *= binding->array.dims[i_dim];

	// runtime array
	if (set_binding.descriptorCount == 0)
		set_binding.descriptorCount = 10'000;

	return set_binding;
}

void SpvLoader::reflect_shader_stage()
{
	ZoneScoped;

	if (reflection.shader_stage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
		shader.stage = vk::ShaderStageFlagBits::eVertex;
	else if (reflection.shader_stage & SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT)
		shader.stage = vk::ShaderStageFlagBits::eFragment;
	else if (reflection.shader_stage & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT)
		shader.stage = vk::ShaderStageFlagBits::eCompute;
	else
		assert_fail("Reflected shader stage is invalid");
}

} // namespace BINDLESSVK_NAMESPACE
