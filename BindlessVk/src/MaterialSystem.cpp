#include "BindlessVk/MaterialSystem.hpp"

#include <spirv_reflect.h>

static_assert(SPV_REFLECT_RESULT_SUCCESS == 0, "SPV_REFLECT_RESULT_SUCCESS was assumed to be 0, but it isn't");

namespace BINDLESSVK_NAMESPACE {

void MaterialSystem::Init(const MaterialSystem::CreateInfo& info)
{
	m_Device = info.device;

	std::vector<vk::DescriptorPoolSize> poolSizes = {
		{ vk::DescriptorType::eSampler, 1000 },
		{ vk::DescriptorType::eCombinedImageSampler, 1000 },
		{ vk::DescriptorType::eSampledImage, 1000 },
		{ vk::DescriptorType::eStorageImage, 1000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		{ vk::DescriptorType::eUniformBuffer, 1000 },
		{ vk::DescriptorType::eStorageBuffer, 1000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		{ vk::DescriptorType::eInputAttachment, 1000 }
	};
	// descriptorPoolSizes.push_back(VkDescriptorPoolSize {});

	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {
		vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		100,                                     // maxSets
		static_cast<uint32_t>(poolSizes.size()), // poolSizeCount
		poolSizes.data(),                        // pPoolSizes
	};

	m_DescriptorPool = m_Device->logical.createDescriptorPool(descriptorPoolCreateInfo, nullptr);

	const vk::Extent2D& extent = m_Device->framebufferExtent;
}

void MaterialSystem::Reset()
{
	DestroyAllMaterials();

	m_Device->logical.destroyDescriptorPool(m_DescriptorPool);

	for (auto& [key, val] : m_ShaderEffects)
	{
		m_Device->logical.destroyDescriptorSetLayout(val.setsLayout[0]);
		m_Device->logical.destroyDescriptorSetLayout(val.setsLayout[1]);
		m_Device->logical.destroyPipelineLayout(val.pipelineLayout);

		static_assert(ShaderEffect().setsLayout.size() == 2, "Sets layout has been resized");
	}

	for (auto& [key, val] : m_Shaders)
	{
		m_Device->logical.destroyShaderModule(val.module);
	}


	m_ShaderEffects.clear();
	m_Shaders.clear();
}

void MaterialSystem::DestroyAllMaterials()
{
	for (auto& [key, val] : m_ShaderPasses)
	{
		m_Device->logical.destroyPipeline(val.pipeline);
	}

	m_Device->logical.resetDescriptorPool(m_DescriptorPool);

	m_ShaderPasses.clear();
	m_Materials.clear();
}

void MaterialSystem::LoadShader(const Shader::CreateInfo& info)
{
	std::string test(info.name);
	std::ifstream stream(info.path, std::ios::ate);
	test = std::string(info.name);

	const size_t fileSize = stream.tellg();
	std::vector<uint32_t> code(fileSize / sizeof(uint32_t));

	stream.seekg(0);
	stream.read((char*)code.data(), fileSize);
	stream.close();

	vk::ShaderModuleCreateInfo createInfo {
		{},                             // flags
		code.size() * sizeof(uint32_t), // codeSize
		code.data(),                    // code
	};

	test                          = std::string(info.name);
	m_Shaders[HashStr(info.name)] = {
		m_Device->logical.createShaderModule(createInfo), // module
		info.stage,                                       // stage
		code,                                             // code
	};
}

void MaterialSystem::CreateShaderEffect(const ShaderEffect::CreateInfo& info)
{
	std::array<std::vector<vk::DescriptorSetLayoutBinding>, 2> setBindings;

	for (const auto& shaderStage : info.shaders)
	{
		SpvReflectShaderModule spvModule;

		BVK_ASSERT(spvReflectCreateShaderModule(shaderStage->code.size() * sizeof(uint32_t), shaderStage->code.data(), &spvModule), "spvReflectCreateShaderModule failed");

		uint32_t descriptorSetsCount = 0ul;
		BVK_ASSERT(spvReflectEnumerateDescriptorSets(&spvModule, &descriptorSetsCount, nullptr), "spvReflectEnumerateDescriptorSets failed");

		std::vector<SpvReflectDescriptorSet*> descriptorSets(descriptorSetsCount);
		BVK_ASSERT(spvReflectEnumerateDescriptorSets(&spvModule, &descriptorSetsCount, descriptorSets.data()), "spvReflectEnumerateDescriptorSets failed");

		for (const auto& spvSet : descriptorSets)
		{
			for (uint32_t i_binding = 0ull; i_binding < spvSet->binding_count; i_binding++)
			{
				const auto& spvBinding = *(spvSet->bindings[i_binding]);
				if (setBindings[spvSet->set].size() < spvBinding.binding + 1)
				{
					setBindings[spvSet->set].resize(spvBinding.binding + 1);
				}

				setBindings[spvBinding.set][spvBinding.binding].binding        = spvBinding.binding;
				setBindings[spvBinding.set][spvBinding.binding].descriptorType = static_cast<vk::DescriptorType>(spvBinding.descriptor_type);
				setBindings[spvBinding.set][spvBinding.binding].stageFlags     = shaderStage->stage;

				setBindings[spvBinding.set][spvBinding.binding].descriptorCount = 1u;
				for (uint32_t i_dim = 0; i_dim < spvBinding.array.dims_count; i_dim++)
				{
					setBindings[spvBinding.set][spvBinding.binding].descriptorCount *= spvBinding.array.dims[i_dim];
				}
			}
		}
	}

	std::array<vk::DescriptorSetLayout, 2ull> setsLayout;
	uint32_t index = 0ull;
	for (const auto& set : setBindings)
	{
		vk::DescriptorSetLayoutCreateInfo setLayoutCreateInfo {
			{},                                // flags
			static_cast<uint32_t>(set.size()), // bindingCount
			set.data(),                        // pBindings
		};
		setsLayout[index++] = m_Device->logical.createDescriptorSetLayout(setLayoutCreateInfo);
	}
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		{},                                       // flags
		static_cast<uint32_t>(setsLayout.size()), // setLayoutCount
		setsLayout.data(),                        // pSetLayouts
	};


	m_ShaderEffects[HashStr(info.name)] = {
		info.shaders,                                                     // shaders
		m_Device->logical.createPipelineLayout(pipelineLayoutCreateInfo), // piplineLayout
		setsLayout                                                        // setsLayout
	};
}

void MaterialSystem::CreateShaderPass(const ShaderPass::CreateInfo& info)
{
	if (m_ShaderPasses.contains(HashStr(info.name)))
	{
		BVK_LOG(LogLvl::eWarn, "Recreating shader pass: {}", info.name);
		m_Device->logical.destroyPipeline(m_ShaderPasses[HashStr(info.name)].pipeline);
	}

	std::vector<vk::PipelineShaderStageCreateInfo> stages(info.effect->shaders.size());

	uint32_t index = 0;
	for (const auto& shader : info.effect->shaders)
	{
		stages[index++] = {
			{}, //flags
			shader->stage,
			shader->module,
			"main",
		};
	}

	vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo {
		{},                          //  viewMask
		1u,                          // colorAttachmentCount
		&info.colorAttachmentFormat, // pColorAttachmentFormats
		info.depthAttachmentFormat,  // depthAttachmentFormat
		{},                          // stencilAttachmentFormat
	};

	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo {
		{}, // flags
		static_cast<uint32_t>(stages.size()),
		stages.data(),
		&info.pipelineConfiguration.vertexInputState,
		&info.pipelineConfiguration.inputAssemblyState,
		&info.pipelineConfiguration.tessellationState,
		&info.pipelineConfiguration.viewportState,
		&info.pipelineConfiguration.rasterizationState,
		&info.pipelineConfiguration.multisampleState,
		&info.pipelineConfiguration.depthStencilState,
		&info.pipelineConfiguration.colorBlendState,
		&info.pipelineConfiguration.dynamicState,
		info.effect->pipelineLayout,
		{}, // renderPass
		{}, // subpass
		{}, // basePipelineHandle
		{}, // basePipelineIndex
		&pipelineRenderingCreateInfo,
	};

	auto pipeline = m_Device->logical.createGraphicsPipeline({}, graphicsPipelineCreateInfo);
	BVK_ASSERT(pipeline.result);

	m_ShaderPasses[HashStr(info.name)] = {
		info.effect,
		pipeline.value,
	};
}

void MaterialSystem::CreatePipelineConfiguration(const PipelineConfiguration::CreateInfo& info)
{
	PipelineConfiguration& configuration = m_PipelineConfigurations[HashStr(info.name)];


	configuration = PipelineConfiguration {
		.vertexInputState   = info.vertexInputState,
		.inputAssemblyState = info.inputAssemblyState,
		.tessellationState  = info.tessellationState,
		.viewportState      = info.viewportState,
		.rasterizationState = info.rasterizationState,
		.multisampleState   = info.multisampleState,
		.depthStencilState  = info.depthStencilState,

		.colorBlendAttachments = info.colorBlendAttachments,
		.colorBlendState       = info.colorBlendState,

		.dynamicStates = info.dynamicStates,
	};

	configuration.dynamicState = {
		{},
		static_cast<uint32_t>(configuration.dynamicStates.size()),
		configuration.dynamicStates.data(),
	};

	configuration.colorBlendState.attachmentCount = configuration.colorBlendAttachments.size();
	configuration.colorBlendState.pAttachments    = configuration.colorBlendAttachments.data();
}

void MaterialSystem::CreateMaterial(const Material::CreateInfo& info)
{
	vk::DescriptorSetAllocateInfo allocateInfo {
		m_DescriptorPool, // descriptorPool
		1ull,             // descriptorSetCount
		&info.shaderPass->effect->setsLayout.back(),
	};

	if (m_Materials.contains(HashStr(info.name)))
	{
		m_Device->logical.freeDescriptorSets(m_DescriptorPool, m_Materials[HashStr(info.name)].descriptorSet);
		BVK_LOG(LogLvl::eWarn, "Recreating material: {}", info.name);
	}

	vk::DescriptorSet set;
	BVK_ASSERT(m_Device->logical.allocateDescriptorSets(&allocateInfo, &set));

	m_Materials[HashStr(info.name)] = {
		.shaderPass    = info.shaderPass,
		.parameters    = info.parameters,
		.descriptorSet = set,
		.textures      = info.textures,
		.sortKey       = static_cast<uint32_t>(HashStr(info.name)),
	};

	// @todo Bind textures
}

} // namespace BINDLESSVK_NAMESPACE
