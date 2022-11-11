#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include "Core/Base.hpp"
#include "Graphics/Buffer.hpp"
#include "Graphics/Device.hpp"
#include "Graphics/Model.hpp"
#include "Scene/CameraController.hpp"
// #include "Graphics/Pipeline.hpp"
#include "Graphics/RenderGraph.hpp"
#include "Graphics/RenderPass.hpp"
#include "Graphics/Texture.hpp"
#include "Graphics/Types.hpp"

#include <functional>
#include <vector>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#ifndef MAX_FRAMES_IN_FLIGHT
	#define MAX_FRAMES_IN_FLIGHT 3
#endif

#ifndef DESIRED_SWAPCHAIN_IMAGES
	#define DESIRED_SWAPCHAIN_IMAGES 3
#endif

class Window;

class Renderer
{
public:
	struct CreateInfo
	{
		DeviceContext deviceContext;
		Window* window;
		Texture* defaultTexture;
		Texture* skyboxTexture;
	};

public:
	Renderer(const Renderer::CreateInfo& createInfo);
	~Renderer();

	void RecreateSwapchainResources(Window* window, DeviceContext context);
	void DestroySwapchain();

	void BeginFrame();
	void Draw();
	void DrawScene(class Scene* scene);


	void ImmediateSubmit(std::function<void(vk::CommandBuffer)>&& function);

	inline QueueInfo GetQueueInfo() const { return m_QueueInfo; }

	inline vk::CommandPool GetCommandPool() const { return m_CommandPool; }
	inline uint32_t GetImageCount() const { return m_SwapchainImages.size(); }

	inline bool IsSwapchainInvalidated() const { return m_SwapchainInvalidated; }

private:
	struct UploadContext
	{
		vk::CommandBuffer cmdBuffer;
		vk::CommandPool cmdPool;
		vk::Fence fence;
	};

private:
	void CreateSyncObjects();
	void CreateDescriptorPools();

	void CreateSwapchain();

	void CreateCommandPool();
	void InitializeImGui(const DeviceContext& deviceContext, Window* window);
	void CreateRenderGraph(const DeviceContext& deviceContex);

	void SubmitQueue(vk::Semaphore waitSemaphore, vk::Semaphore signalSemaphore, vk::Fence signalFence, vk::CommandBuffer cmd);
	void PresentFrame(vk::Semaphore waitSemaphore, uint32_t imageIndex);

private:
	UploadContext m_UploadContext = {};
	vk::Device m_LogicalDevice    = {};
	QueueInfo m_QueueInfo         = {};
	SurfaceInfo m_SurfaceInfo     = {};
	vk::Format m_DepthFormat      = {};

	vma::Allocator m_Allocator = {};

	RenderGraph m_RenderGraph;

	// Sync objects
	std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> m_RenderFences          = {};
	std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> m_RenderSemaphores  = {};
	std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> m_PresentSemaphores = {};

	bool m_SwapchainInvalidated = false;

	// Swapchain
	vk::SwapchainKHR m_Swapchain = {};

	std::vector<vk::Image> m_SwapchainImages         = {};
	std::vector<vk::ImageView> m_SwapchainImageViews = {};


	RenderPass m_ForwardPass;
	RenderPass m_UIPass;

	// Render Targets
	AllocatedImage m_ColorTarget    = {};
	vk::ImageView m_ColorTargetView = {};

	AllocatedImage m_DepthTarget    = {};
	vk::ImageView m_DepthTargetView = {};

	vk::SampleCountFlagBits m_SampleCount = vk::SampleCountFlagBits::e1;

	// Pools
	vk::CommandPool m_CommandPool       = {};
	vk::DescriptorPool m_DescriptorPool = {};

	std::vector<vk::CommandBuffer> m_CommandBuffers = {};

	uint32_t m_CurrentFrame = 0ul;

	Texture* m_DefaultTexture = {};
	Texture* m_SkyboxTexture  = {};
};
