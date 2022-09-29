#pragma once

#include "Core/Base.hpp"

#include <glm/glm.hpp>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

class Window;

struct UniformMVP
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct SurfaceInfo
{
	vk::SurfaceKHR surface = VK_NULL_HANDLE;
	vk::SurfaceCapabilitiesKHR capabilities;
	vk::SurfaceFormatKHR format;
	vk::PresentModeKHR presentMode;

	std::vector<vk::SurfaceFormatKHR> supportedFormats;
	std::vector<vk::PresentModeKHR> supportedPresentModes;
};

struct QueueInfo
{
	// WARN!: These should be coherent in memory
	uint32_t graphicsQueueIndex = UINT32_MAX;
	uint32_t presentQueueIndex  = UINT32_MAX;

	vk::Queue graphicsQueue = VK_NULL_HANDLE;
	vk::Queue presentQueue  = VK_NULL_HANDLE;
};

struct DeviceCreateInfo
{
	Window* window;
	std::vector<const char*> layers;
	std::vector<const char*> instanceExtensions;
	std::vector<const char*> logicalDeviceExtensions;

	bool enableDebugging;
	vk::DebugUtilsMessageSeverityFlagBitsEXT debugMessageSeverity;
	vk::DebugUtilsMessageTypeFlagsEXT debugMessageTypes;
};

class Device
{
public:
	Device(DeviceCreateInfo& createInfo);
	~Device();

	void LogDebugInfo();

	SurfaceInfo FetchSurfaceInfo();

	void DrawFrame();

	inline QueueInfo GetQueueInfo() const { return m_QueueInfo; }

	inline PFN_vkGetInstanceProcAddr GetInstanceProcAddr() const { return m_GetInstanceProcAddr; }
	inline vk::Instance GetInstance() const { return m_Instance; }
	inline vk::Device GetLogicalDevice() const { return m_LogicalDevice; }
	inline vk::PhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
	inline vk::PhysicalDeviceProperties GetPhysicalDeviceProperties() const { return m_PhysicalDeviceProperties; }
	inline vk::SampleCountFlagBits GetMaxSupportedSampleCount() const { return m_MaxSupportedSampleCount; }

	inline vma::Allocator GetAllocator() const { return m_Allocator; }

private:
	vk::DynamicLoader m_DynamicLoader;
	PFN_vkGetInstanceProcAddr m_GetInstanceProcAddr;

	// Instance
	vk::Instance m_Instance = {};

	// Layers & Extensions
	std::vector<const char*> m_Layers     = {};
	std::vector<const char*> m_Extensions = {};

	// Device
	vk::Device m_LogicalDevice                              = {};
	vk::PhysicalDevice m_PhysicalDevice                     = {};
	vk::PhysicalDeviceProperties m_PhysicalDeviceProperties = {};
	vk::SampleCountFlagBits m_MaxSupportedSampleCount       = vk::SampleCountFlagBits::e1;

	// Queue & Surface
	QueueInfo m_QueueInfo     = {};
	SurfaceInfo m_SurfaceInfo = {};

	vma::Allocator m_Allocator = {};
};
