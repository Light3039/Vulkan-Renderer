#include "Rendergraphs/Passes/Forward.hpp"

#include "Rendergraphs/Graph.hpp"

#include <BindlessVk/Renderer/Rendergraph.hpp>
#include <imgui.h>

Forwardpass::Forwardpass(bvk::VkContext const *const vk_context)
    : bvk::RenderNode(vk_context)
    , device(vk_context->get_device())
    , tracy_graphics(vk_context->get_tracy_graphics())
    , tracy_compute(vk_context->get_tracy_compute())
{
}

void Forwardpass::on_setup(bvk::RenderNode *parent)
{
	auto const data = any_cast<UserData *>(user_data);

	scene = data->scene;
	memory_allocator = data->memory_allocator;

	draw_indirect_buffer = &parent->get_buffer_inputs().at(
	    BasicRendergraph::DrawIndirectDescriptor::key
	);

	cull_pipeline = data->cull_pipeline;
	model_pipeline = data->model_pipeline;
	skybox_pipeline = data->skybox_pipeline;

	scene->view<StaticMeshComponent const>().each([this](StaticMeshComponent const &mesh) {
		auto model = mesh.model;

		for (auto const *node : model->get_nodes())
			primitive_count += node->mesh.size();
	});
}

void Forwardpass::on_frame_prepare(u32 frame_index, u32 image_index)
{
	static_mesh_count = scene->view<StaticMeshComponent>().size();
}

void Forwardpass::on_frame_compute(vk::CommandBuffer cmd, u32 frame_index, u32 image_index)
{
	TracyVkZone(tracy_compute.context, cmd, "culling");

	ImGui::Begin("Forwardpass options");

	ImGui::Checkbox("freeze frustum culling", &freeze_cull);

	if (!freeze_cull)
	{
		u32 dispatch_x = 1 + (primitive_count / 64);
		ImGui::Text("dispatches: %u", dispatch_x);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, cull_pipeline->get_pipeline());
		cmd.dispatch(dispatch_x, 1, 1);
	}

	ImGui::End();
}

void Forwardpass::on_frame_graphics(
    vk::CommandBuffer const cmd,
    u32 const frame_index,
    u32 const image_index
)
{
	auto const &surface = vk_context->get_surface();

	this->cmd = cmd;
	current_pipeline = vk::Pipeline {};

	render_static_meshes(frame_index);
	render_skyboxes();
}

void Forwardpass::render_static_meshes(u32 frame_index)
{
	TracyVkZone(tracy_graphics.context, cmd, "render_static_meshes");
	switch_pipeline(model_pipeline->get_pipeline());

	ImGui::Text("primitives: %u", primitive_count);

	cmd.drawIndexedIndirect(
	    *draw_indirect_buffer->vk(),
	    0,
	    primitive_count,
	    sizeof(BasicRendergraph::DrawIndirectDescriptor)
	);
}

void Forwardpass::render_skyboxes()
{
	TracyVkZone(tracy_graphics.context, cmd, "render_skybox");

	switch_pipeline(skybox_pipeline->get_pipeline());

	auto const skyboxes = scene->view<const SkyboxComponent>();
	skyboxes.each([this](auto const &skybox) { render_skybox(skybox); });
}

void Forwardpass::render_static_mesh(StaticMeshComponent const &static_mesh, u32 &index)
{
	draw_model(static_mesh.model, index);
}

void Forwardpass::render_skybox(SkyboxComponent const &skybox)
{
	u32 primitive_index = 0;
	draw_model(skybox.model, primitive_index);
}

void Forwardpass::switch_pipeline(vk::Pipeline pipeline)
{
	auto const extent = vk_context->get_surface()->get_framebuffer_extent();
	auto const [width, height] = extent;

	TracyVkZone(tracy_graphics.context, cmd, "switch_pipeline");

	current_pipeline = pipeline;
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, current_pipeline);

	cmd.setScissor(
	    0,
	    {
	        {

	            { 0, 0 },
	            extent,
	        },
	    }
	);

	cmd.setViewport(
	    0,
	    {
	        {
	            0.0f,
	            0.0f,
	            f32(width),
	            f32(height),
	            0.0f,
	            1.0f,
	        },
	    }
	);
}

void Forwardpass::draw_model(bvk::Model const *const model, u32 &primitive_index)
{
	TracyVkZone(tracy_graphics.context, cmd, "draw_model");

	auto const index_offset = model->get_index_offset();
	auto const vertex_offset = model->get_vertex_offset();

	for (auto const *node : model->get_nodes())
		for (auto const &primitive : node->mesh)
			cmd.drawIndexed(
			    primitive.index_count,
			    1,
			    primitive.first_index + index_offset,
			    vertex_offset,
			    primitive_index++
			);
}
