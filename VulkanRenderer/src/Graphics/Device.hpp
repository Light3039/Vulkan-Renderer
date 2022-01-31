#pragma once

#include "Core/Base.hpp"

#include <volk.h>

struct DeviceCreateInfo
{
	std::vector<const char*> layers;
	std::vector<const char*> extensions;

	bool enableDebugging;
	VkDebugUtilsMessageSeverityFlagBitsEXT minMessageSeverity;
	VkDebugUtilsMessageTypeFlagsEXT messageTypes;
};

class Device
{
public:
	Device(DeviceCreateInfo& createInfo);
	~Device();

private:
	// Instance
	VkInstance m_Instance = VK_NULL_HANDLE;

	// Layers & Extensions
	std::vector<const char*> m_Layers;
	std::vector<const char*> m_Extensions;

	void CreateInstance(VkDebugUtilsMessengerCreateInfoEXT debugCallbackCreateInfo);

	VkDebugUtilsMessengerCreateInfoEXT CreateDebugCallbackCreateInfo();
	bool CheckLayersSupport();
	bool CheckExtensionsSupport();
};
