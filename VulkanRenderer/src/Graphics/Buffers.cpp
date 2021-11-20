#include "Graphics/Buffers.h"

#include "Graphics/Device.h"

Buffer::Buffer(class Device* device, uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
    : m_Device(device)
    , m_Buffer(VK_NULL_HANDLE)
    , m_Memory(VK_NULL_HANDLE)
{
    // buffer create-info
    VkBufferCreateInfo bufferCreateInfo {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    // create buffer
    VKC(vkCreateBuffer(m_Device->logical(), &bufferCreateInfo, nullptr, &m_Buffer));

    VkMemoryRequirements memoryRequirments;
    vkGetBufferMemoryRequirements(m_Device->logical(), m_Buffer, &memoryRequirments);

    VkMemoryAllocateInfo allocateInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memoryRequirments.size,
        .memoryTypeIndex = FetchMemoryType(memoryRequirments.memoryTypeBits, memoryProperties),
    };

    VKC(vkAllocateMemory(m_Device->logical(), &allocateInfo, nullptr, &m_Memory));
    VKC(vkBindBufferMemory(m_Device->logical(), m_Buffer, m_Memory, 0u));
}

Buffer::~Buffer()
{
    vkDestroyBuffer(m_Device->logical(), m_Buffer, nullptr);
    vkFreeMemory(m_Device->logical(), m_Memory, nullptr);
}

void Buffer::CopyBufferToSelf(Buffer* src, uint32_t size, VkCommandPool commandPool, VkQueue graphicsQueue)
{
    VkCommandBufferAllocateInfo allocInfo {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1u,
    };

    VkCommandBuffer commandBuffer;
    VKC(vkAllocateCommandBuffers(m_Device->logical(), &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VKC(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkBufferCopy copyRegion {
        .srcOffset = 0u,
        .dstOffset = 0u,
        .size      = size
    };

    vkCmdCopyBuffer(commandBuffer, *src->GetBuffer(), m_Buffer, 1u, &copyRegion);
    VKC(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo copySubmitInfo {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1u,
        .pCommandBuffers    = &commandBuffer
    };

    VKC(vkQueueSubmit(graphicsQueue, 1u, &copySubmitInfo, VK_NULL_HANDLE));
    VKC(vkQueueWaitIdle(graphicsQueue));

    vkFreeCommandBuffers(m_Device->logical(), commandPool, 1u, &commandBuffer);
}

void* Buffer::Map(uint32_t size)
{
    void* map;
    VKC(vkMapMemory(m_Device->logical(), m_Memory, NULL, size, NULL, &map));
    return map;
}

void Buffer::Unmap()
{
    vkUnmapMemory(m_Device->logical(), m_Memory);
}

uint32_t Buffer::FetchMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties physicalMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(m_Device->physical(), &physicalMemoryProperties);

    for (uint32_t i = 0; i < physicalMemoryProperties.memoryTypeCount; i++)
    {
        if (typeFilter & (1 << i) && (physicalMemoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to fetch required memory type!");
    return -1;
}
