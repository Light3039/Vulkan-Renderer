#include "Debug/UserInterface.h"

#include "Graphics/Device.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

UserInterface::UserInterface(GLFWwindow* window, Device* device, VkRenderPass renderPass)
{
	// create vulkan stuff
	VkDescriptorPoolSize poolSize[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolCreateInfo {
		.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets       = 1000u * IM_ARRAYSIZE(poolSize),
		.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSize)),
		.pPoolSizes    = poolSize
	};

	VKC(vkCreateDescriptorPool(device->logical(), &poolCreateInfo, nullptr, &m_DescriptorPool));
	vkDeviceWaitIdle(device->logical());

	// create context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// io confi    ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding              = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// vulkan implementation
	ImGui_ImplGlfw_InitForVulkan(window, true);
	ImGui_ImplVulkan_InitInfo initInfo {
		.Instance        = device->instance(),
		.PhysicalDevice  = device->physical(),
		.Device          = device->logical(),
		.QueueFamily     = device->graphicsQueueIndex(),
		.Queue           = device->graphicsQueue(),
		.PipelineCache   = VK_NULL_HANDLE,
		.DescriptorPool  = m_DescriptorPool,
		.MinImageCount   = 2u,
		.ImageCount      = 2u,
		.Allocator       = nullptr,
		.CheckVkResultFn = [](VkResult result) { VKC(result); },
	};

	ImGui_ImplVulkan_Init(&initInfo, renderPass);
	VkCommandPool commandPool;
	// command pool create-info
	VkCommandPoolCreateInfo commandpoolCreateInfo {
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = device->graphicsQueueIndex(),
	};

	VKC(vkCreateCommandPool(device->logical(), &commandpoolCreateInfo, nullptr, &commandPool));
	// Use any command queue
	// command buffer allocate-info
	VkCommandBufferAllocateInfo commandBufferAllocateInfo {
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool        = commandPool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VkCommandBuffer commandBuffer;
	VKC(vkAllocateCommandBuffers(device->logical(), &commandBufferAllocateInfo, &commandBuffer));

	VKC(vkResetCommandPool(device->logical(), commandPool, 0));
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VKC(vkBeginCommandBuffer(commandBuffer, &begin_info));

	ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

	VkSubmitInfo end_info       = {};
	end_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	end_info.commandBufferCount = 1;
	end_info.pCommandBuffers    = &commandBuffer;
	VKC(vkEndCommandBuffer(commandBuffer));
	VKC(vkQueueSubmit(device->graphicsQueue(), 1, &end_info, VK_NULL_HANDLE));
	VKC(vkDeviceWaitIdle(device->logical()));
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

UserInterface::~UserInterface()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
void UserInterface::Begin()
{
	// render user interface
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void UserInterface::End()
{
	static bool showDemo = true;
	ImGui::ShowDemoWindow(&showDemo);

	ImGui::Render();
}

class ImDrawData* UserInterface::GetDrawData()
{
	return ImGui::GetDrawData();
}
