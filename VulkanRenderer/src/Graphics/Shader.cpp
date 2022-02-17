#include "Graphics/Shader.hpp"
#include <fstream>

Shader::Shader(ShaderCreateInfo& createInfo)
    : m_LogicalDevice(createInfo.logicalDevice)
{
	/////////////////////////////////////////////////////////////////////////////////
	// Vertex shader
	auto vertexSpv = CompileGlslToSpv(createInfo.vertexPath, Stage::Vertex, createInfo.optimizationLevel);

	VkShaderModuleCreateInfo vertexCreateInfo {
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<size_t>(vertexSpv.end() - vertexSpv.begin()) * 4ull,
		.pCode    = vertexSpv.begin(),
	};
	VKC(vkCreateShaderModule(m_LogicalDevice, &vertexCreateInfo, nullptr, &m_VertexShaderModule));

	m_PipelineShaderCreateInfos[0] = {
		.sType  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.stage  = VK_SHADER_STAGE_VERTEX_BIT,
		.module = m_VertexShaderModule,
		.pName  = "main",
	};

	/////////////////////////////////////////////////////////////////////////////////
	// Pixel shader
	auto pixelSpv = CompileGlslToSpv(createInfo.pixelPath, Stage::Pixel, createInfo.optimizationLevel);

	VkShaderModuleCreateInfo pixelCreateInfo {
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<size_t>(pixelSpv.end() - pixelSpv.begin()) * 4ull,
		.pCode    = pixelSpv.begin(),
	};
	VKC(vkCreateShaderModule(m_LogicalDevice, &pixelCreateInfo, nullptr, &m_PixelShaderModule));

	m_PipelineShaderCreateInfos[1] = {
		.sType  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = m_PixelShaderModule,
		.pName  = "main",
	};
}

Shader::~Shader()
{
	vkDestroyShaderModule(m_LogicalDevice, m_VertexShaderModule, nullptr);
	vkDestroyShaderModule(m_LogicalDevice, m_PixelShaderModule, nullptr);
}

shaderc::SpvCompilationResult Shader::CompileGlslToSpv(const std::string& path, Stage stage, shaderc_optimization_level optimizationLevel)
{
	// compile options
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
	options.SetOptimizationLevel(optimizationLevel);

	// read shader file
	std::ifstream file(path, std::ios::ate | std::ios::binary | std::ios::in);
	std::string sourceText = "";
	std::string fileName   = path.substr(path.find_last_of("/") + 1);
	if (file)
	{
		sourceText.resize(static_cast<size_t>(file.tellg()));
		file.seekg(0, std::ios::beg);
		file.read(&sourceText[0], sourceText.size());
		sourceText.resize(static_cast<size_t>(file.gcount()));
		file.close();
	}

	// compile
	shaderc::Compiler compiler;
	shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(sourceText, stage == Stage::Vertex ? shaderc_shader_kind::shaderc_vertex_shader : shaderc_shader_kind::shaderc_fragment_shader, fileName.c_str(), options);

	// log error
	if (result.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		LOG(err, "Failed to compile {} shader at {}...", stage == Stage::Vertex ? "vertex" : "pixel", path);
		LOG(err, "    {}", result.GetErrorMessage());
	}

	return result;
}

