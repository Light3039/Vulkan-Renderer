#include "Rendergraphs/Passes/UserInterface.hpp"

#include "Framework/UserInterface/ImguiGlfwBackend.hpp"
#include "Framework/UserInterface/ImguiVulkanBackend.hpp"

#include <imgui.h>


UserInterfacePass::UserInterfacePass(bvk::VkContext const *const vk_context)
    : bvk::Renderpass(vk_context)
{
}

void UserInterfacePass::on_update(u32 const frame_index, u32 const image_index)
{
	ImGui::Render();
}

void UserInterfacePass::on_render(
    vk::CommandBuffer const cmd,
    u32 const frame_index,
    u32 const image_index
)
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}
