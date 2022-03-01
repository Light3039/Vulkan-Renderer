#pragma once

#include "Core/Base.hpp"
#include "Graphics/Buffer.hpp"
#include "Graphics/Shader.hpp"

#include <volk.h>

struct PipelineCreateInfo
{
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;
	VkQueue graphicsQueue;
	VkExtent2D viewportExtent;
	VkCommandPool commandPool;
	uint32_t imageCount;
	VkRenderPass renderPass;

	// Shader
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	const std::string vertexShaderPath;
	const std::string pixelShaderPath;

	VkVertexInputBindingDescription vertexBindingDesc;
	std::vector<VkVertexInputAttributeDescription> vertexAttribDescs;
};

struct CommandBufferStartInfo
{
	VkDescriptorSet* mvpDescriptorSet;
	VkFramebuffer framebuffer;
	VkExtent2D extent;
	uint32_t frameIndex;
};

class Pipeline
{
public:
	Pipeline(PipelineCreateInfo& createInfo);
	~Pipeline();

	VkCommandBuffer RecordCommandBuffer(CommandBufferStartInfo& startInfo);

private:
	VkDevice m_LogicalDevice;

	VkPipeline m_Pipeline;
	VkPipelineLayout m_PipelineLayout;
	VkRenderPass m_RenderPass;

	std::unique_ptr<Shader> m_Shader;
	std::unique_ptr<StagingBuffer> m_VertexBuffer;
	std::unique_ptr<StagingBuffer> m_IndexBuffer;

	std::vector<VkCommandBuffer> m_CommandBuffers;
	std::vector<StagingBuffer> m_UniformBuffers;
};
