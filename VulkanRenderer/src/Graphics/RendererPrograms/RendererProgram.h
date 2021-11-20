#pragma once

#include "Core/Base.h"
#include "Graphics/Buffers.h"
#include "Graphics/DeviceContext.h"
#include "Graphics/Shader.h"

#include <volk.h>

class RendererProgram
{
protected:
    class Device* m_Device;

    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;

    VkCommandPool                m_CommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    VkPipeline       m_Pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    VkViewport m_Viewport;
    VkRect2D   m_Scissors;

    std::unique_ptr<Shader> m_Shader = nullptr;

    std::unique_ptr<Buffer> m_VertexBuffer = nullptr;
    std::unique_ptr<Buffer> m_IndexBuffer  = nullptr;

    std::unique_ptr<Buffer> m_StagingVertexBuffer = nullptr;
    std::unique_ptr<Buffer> m_StagingIndexBuffer  = nullptr;

public:
    RendererProgram(class Device* device, VkCommandPool commandPool, VkQueue graphicsQueue);

    virtual ~RendererProgram();
};