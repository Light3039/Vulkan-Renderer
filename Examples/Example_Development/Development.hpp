#pragma once

#include "BindlessVk/RenderGraph.hpp"
#include "BindlessVk/RenderPass.hpp"
#include "BindlessVk/Renderer.hpp"
#include "BindlessVk/VkContext.hpp"
#include "Framework/Common/Common.hpp"
//
#include "Framework/Core/Application.hpp"
#include "Framework/Core/Window.hpp"
#include "Framework/Scene/CameraController.hpp"
#include "Framework/Scene/Scene.hpp"
#include "Framework/Utils/Timer.hpp"

// passes
#include "ForwardPass.hpp"
#include "Graph.hpp"
#include "UserInterfacePass.hpp"

// @todo: load stuff from files
class DevelopmentExampleApplication: public Application
{
public:
	DevelopmentExampleApplication(): Application()
	{
		load_shaders();
		load_pipeline_configuration();
		load_shader_effects();
		load_materials();

		load_models();

		load_entities();

		create_render_graph();
	}

	~DevelopmentExampleApplication()
	{
	}

	virtual void on_tick(f64 const delta_time) override
	{
		ImGui::ShowDemoWindow();
		CVar::draw_imgui_editor();

		camera_controller.update();

		auto *const swapchain = vk_context->get_swapchain();

		renderer->render_graph(&render_graph, &scene);

		if (swapchain->is_invalid()) {
			assert_fail("swapchain-recreation is currently nuked");

			// logger.log(spdlog::level::trace, "Swapchain invalidated");
			//
			// vk_context->update_surface_info();
			//
			// camera_controller.on_window_resize(
			//     window.get_framebuffer_size().width,
			//     window.get_framebuffer_size().height
			// );
			//
			// load_materials();
			//
			// render_graph->on_swapchain_invalidated(
			//     swapchain->get_images(),
			//     swapchain->get_image_views()
			// );
		}
	}

	virtual void on_swapchain_recreate() override
	{
		assert_fail("Swapchain recreation not supported (yet)");
	}

private:
	void load_shaders()
	{
		c_str DIRECTORY = "Shaders/";

		for (auto const &shader_file : std::filesystem::directory_iterator(DIRECTORY)) {
			const str path(shader_file.path().c_str());
			const str name(shader_file.path().filename().replace_extension().c_str());
			const str extension(shader_file.path().extension().c_str());

			if (strcmp(extension.c_str(), ".spv")) {
				continue;
			}

			shaders[hash_str(name.c_str())] = shader_loader.load_from_spv(path.c_str());
			logger.log(spdlog::level::trace, "Loaded shader {}", name);
		}
	}

	void load_shader_effects()
	{
		shader_effects[hash_str("opaque_mesh")] = bvk::ShaderEffect(
		    vk_context.get(),
		    {
		        &shaders[hash_str("vertex")],
		        &shaders[hash_str("pixel")],
		    },
		    shader_effect_configurations[hash_str("opaque_mesh")]
		);

		shader_effects[hash_str("skybox")] = bvk::ShaderEffect(
		    vk_context.get(),
		    {
		        &shaders[hash_str("skybox_vertex")],
		        &shaders[hash_str("skybox_fragment")],
		    },
		    shader_effect_configurations[hash_str("skybox")]
		);
	}

	void load_pipeline_configuration()
	{
		shader_effect_configurations[hash_str("opaque_mesh")] = bvk::ShaderEffect::Configuration {
			bvk::Model::Vertex::get_vertex_input_state(),
			vk::PipelineInputAssemblyStateCreateInfo {
			    {},
			    vk::PrimitiveTopology::eTriangleList,
			    VK_FALSE,
			},
			vk::PipelineTessellationStateCreateInfo {},
			vk::PipelineViewportStateCreateInfo {
			    {},
			    1u,
			    {},
			    1u,
			    {},
			},
			vk::PipelineRasterizationStateCreateInfo {
			    {},
			    VK_FALSE,
			    VK_FALSE,
			    vk::PolygonMode::eFill,
			    vk::CullModeFlagBits::eBack,
			    vk::FrontFace::eClockwise,
			    VK_FALSE,
			    0.0f,
			    0.0f,
			    0.0f,
			    1.0f,
			},
			vk::PipelineMultisampleStateCreateInfo {
			    {},
			    vk_context->get_gpu().get_max_color_and_depth_samples(),
			    VK_FALSE,
			    {},
			    VK_FALSE,
			    VK_FALSE,
			},
			vk::PipelineDepthStencilStateCreateInfo {
			    {},
			    VK_TRUE,
			    VK_TRUE,
			    vk::CompareOp::eLess,
			    VK_FALSE,
			    VK_FALSE,
			    {},
			    {},
			    0.0f,
			    1.0,
			},
			vk::PipelineColorBlendStateCreateInfo {},
			std::vector<vk::PipelineColorBlendAttachmentState> {
			    {
			        VK_FALSE,
			        vk::BlendFactor::eSrcAlpha,
			        vk::BlendFactor::eOneMinusSrcAlpha,
			        vk::BlendOp::eAdd,
			        vk::BlendFactor::eOne,
			        vk::BlendFactor::eZero,
			        vk::BlendOp::eAdd,
			        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
			            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
			    },
			},
			std::vector<vk::DynamicState> {
			    vk::DynamicState::eViewport,
			    vk::DynamicState::eScissor,
			},
		};

		shader_effect_configurations[hash_str("skybox")] = bvk::ShaderEffect::Configuration {
			bvk::Model::Vertex::get_vertex_input_state(),
			vk::PipelineInputAssemblyStateCreateInfo {
			    {},
			    vk::PrimitiveTopology::eTriangleList,
			    VK_FALSE,
			},
			vk::PipelineTessellationStateCreateInfo {},
			vk::PipelineViewportStateCreateInfo {
			    {},
			    1u,
			    {},
			    1u,
			    {},
			},
			vk::PipelineRasterizationStateCreateInfo {
			    {},
			    VK_FALSE,
			    VK_FALSE,
			    vk::PolygonMode::eFill,
			    vk::CullModeFlagBits::eBack,
			    vk::FrontFace::eCounterClockwise,
			    VK_FALSE,
			    0.0f,
			    0.0f,
			    0.0f,
			    1.0f,
			},
			vk::PipelineMultisampleStateCreateInfo {
			    {},
			    vk_context->get_gpu().get_max_color_and_depth_samples(),
			    VK_FALSE,
			    {},
			    VK_FALSE,
			    VK_FALSE,
			},
			vk::PipelineDepthStencilStateCreateInfo {
			    {},
			    VK_TRUE,
			    VK_TRUE,
			    vk::CompareOp::eLessOrEqual,
			    VK_FALSE,
			    VK_FALSE,
			    {},
			    {},
			    0.0f,
			    1.0,
			},
			vk::PipelineColorBlendStateCreateInfo {},
			std::vector<vk::PipelineColorBlendAttachmentState> {
			    {
			        VK_FALSE,
			        vk::BlendFactor::eSrcAlpha,
			        vk::BlendFactor::eOneMinusSrcAlpha,
			        vk::BlendOp::eAdd,
			        vk::BlendFactor::eOne,
			        vk::BlendFactor::eZero,
			        vk::BlendOp::eAdd,
			        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
			            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
			    },
			},
			std::vector<vk::DynamicState> {
			    vk::DynamicState::eViewport,
			    vk::DynamicState::eScissor,
			},
		};
	}

	void load_materials()
	{
		materials.emplace(
		    hash_str("opaque_mesh"),
		    bvk::Material(
		        vk_context.get(),
		        &shader_effects[hash_str("opaque_mesh")],
		        descriptor_pool
		    )
		);

		materials.emplace(
		    hash_str("skybox"),
		    bvk::Material(vk_context.get(), &shader_effects[hash_str("skybox")], descriptor_pool)
		);
	}

	void load_models()
	{
		models.emplace(
		    hash_str("flight_helmet"),
		    model_loader.load_from_gltf_ascii(
		        "flight_helmet",
		        "Assets/FlightHelmet/FlightHelmet.gltf",
		        staging_pool.get_by_index(0u),
		        staging_pool.get_by_index(1u),
		        staging_pool.get_by_index(2u)
		    )
		);

		models.emplace(
		    hash_str("skybox"),
		    model_loader.load_from_gltf_ascii(
		        "skybox",
		        "Assets/Cube/Cube.gltf",
		        staging_pool.get_by_index(0u),
		        staging_pool.get_by_index(1u),
		        staging_pool.get_by_index(2u)
		    )
		);
	}

	// @todo: Load from files instead of hard-coding
	void load_entities()
	{
		auto const test_model_entity = scene.create();

		scene.emplace<TransformComponent>(
		    test_model_entity,
		    glm::vec3(0.0f),
		    glm::vec3(1.0f),
		    glm::vec3(0.0f, 0.0, 0.0)
		);

		scene.emplace<StaticMeshRendererComponent>(
		    test_model_entity,
		    &materials.at(hash_str("opaque_mesh")),
		    &models[bvk::hash_str("flight_helmet")]
		);

		auto const skybox_entity = scene.create();
		scene.emplace<TransformComponent>(
		    skybox_entity,
		    glm::vec3(0.0f),
		    glm::vec3(1.0f),
		    glm::vec3(0.0f, 0.0, 0.0)
		);

		scene.emplace<StaticMeshRendererComponent>(
		    skybox_entity,
		    &materials[hash_str("skybox")],
		    &models[bvk::hash_str("skybox")]
		);

		auto const light_entity = scene.create();
		scene.emplace<TransformComponent>(
		    light_entity,
		    glm::vec3(2.0f, 2.0f, 1.0f),
		    glm::vec3(1.0f),
		    glm::vec3(0.0f, 0.0, 0.0)
		);

		auto const camera_entity = scene.create();
		scene.emplace<TransformComponent>(
		    camera_entity,
		    glm::vec3(6.0, 7.0, 2.5),
		    glm::vec3(1.0),
		    glm::vec3(0.0f, 0.0, 0.0)
		);

		scene.emplace<CameraComponent>(
		    camera_entity,
		    45.0f,
		    5.0,
		    1.0,
		    0.001f,
		    100.0f,
		    225.0,
		    0.0,
		    glm::vec3(0.0f, 0.0f, -1.0f),
		    glm::vec3(0.0f, -1.0f, 0.0f),
		    10.0f
		);

		scene.emplace<LightComponent>(light_entity, 12);
	}

	auto create_forward_pass_blueprint() -> bvk::RenderpassBlueprint
	{
		auto const &surface = vk_context->get_surface();

		auto const update_color = arr<f32, 4> { 1.0, 0.8, 0.8, 1.0 };
		auto const render_color = arr<f32, 4> { 0.8, 0.8, 1.0, 1.0 };
		auto const barrier_color = arr<f32, 4> { 0.8, 1.0, 0.8, 1.0 };

		auto const color_format = surface.get_color_format();
		auto const depth_format = vk_context->get_depth_format();
		auto const sample_count = vk_context->get_gpu().get_max_color_and_depth_samples();

		auto const *const default_texture = &textures[hash_str("default_2d")];
		auto const *const default_texture_cube = &textures[hash_str("default_cube")];

		auto blueprint = bvk::RenderpassBlueprint {};
		blueprint.set_name("forwardpass")
		    .set_sample_count(sample_count)
		    .set_update_fn(&forward_pass_update)
		    .set_render_fn(&forward_pass_render)
		    .set_update_label({ "forwardpass_update", update_color })
		    .set_render_label({ "forwardpass_render", render_color })
		    .set_barrier_label({ "forwardpass_barrier", barrier_color })
		    .add_color_attachment({
		        "forwardpass",
		        { 1.0, 1.0 },
		        bvk::Renderpass::SizeType::eSwapchainRelative,
		        color_format,
		        "",
		        "",
		        vk::ClearColorValue { 0.3f, 0.5f, 0.8f, 1.0f },
		    })
		    .set_depth_attachment({
		        "forward_depth",
		        { 1.0, 1.0 },
		        bvk::Renderpass::SizeType::eSwapchainRelative,
		        depth_format,
		        "",
		        "",
		        vk::ClearDepthStencilValue { 1.0, 0 },
		    })
		    .add_texture_input({
		        "texture_2ds",
		        0,
		        32,
		        vk::DescriptorType::eCombinedImageSampler,
		        vk::ShaderStageFlagBits::eFragment,
		        default_texture,
		    })
		    .add_texture_input({
		        "texture_cubes",
		        1,
		        8u,
		        vk::DescriptorType::eCombinedImageSampler,
		        vk::ShaderStageFlagBits::eFragment,
		        default_texture_cube,
		    });

		return blueprint;
	}

	auto create_ui_pass_blueprint() -> bvk::RenderpassBlueprint
	{
		auto const &surface = vk_context->get_surface();

		auto const update_color = arr<f32, 4> { 1.0, 0.8, 0.8, 1.0 };
		auto const render_color = arr<f32, 4> { 0.8, 0.8, 1.0, 1.0 };
		auto const barrier_color = arr<f32, 4> { 0.8, 1.0, 0.8, 1.0 };

		auto const color_format = surface.get_color_format();
		auto const sample_count = vk_context->get_gpu().get_max_color_and_depth_samples();

		auto blueprint = bvk::RenderpassBlueprint {};
		blueprint.set_name("uipass")
		    .set_sample_count(sample_count)
		    .set_update_fn(&user_interface_pass_update)
		    .set_render_fn(&user_interface_pass_render)
		    .set_update_label({ "uipass_update", update_color })
		    .set_render_label({ "uipass_render", render_color })
		    .set_barrier_label({ "uipass_barrier", barrier_color })
		    .add_color_attachment({
		        "uipass_out",
		        { 1.0, 1.0 },
		        bvk::Renderpass::SizeType::eSwapchainRelative,
		        color_format,
		        "forwardpass",
		    });

		return blueprint;
	}

	void create_render_graph()
	{
		auto const update_color = arr<f32, 4> { 1.0, 0.8, 0.8, 1.0 };
		auto const present_color = arr<f32, 4> { 0.8, 1.0, 0.8, 1.0 };

		auto const forwrard_pass_blueprint = create_forward_pass_blueprint();
		auto const ui_pass_blueprint = create_ui_pass_blueprint();

		auto builder = bvk::RenderGraphBuilder { vk_context.get() };
		builder.set_resources(renderer->get_resources())
		    .set_backbuffer_attachment_key(hash_str("uipass_out"))
		    .set_update_fn(&render_graph_update)
		    .set_update_label({ "graph_update", update_color })
		    .set_present_barriers_label({ "graph_present_barriers", present_color })
		    .add_pass(forwrard_pass_blueprint)
		    .add_pass(ui_pass_blueprint)
		    .add_buffer_input({
		        "frame_data",
		        0,
		        1,
		        vk::DescriptorType::eUniformBuffer,
		        vk::ShaderStageFlagBits::eVertex,
		        (sizeof(glm::mat4) * 2) + (sizeof(glm::vec4)),
		        nullptr,
		    })
		    .add_buffer_input({
		        "scene_data",
		        1,
		        1,
		        vk::DescriptorType::eUniformBuffer,
		        vk::ShaderStageFlagBits::eVertex,
		        sizeof(glm::vec4),
		        nullptr,
		    });

		render_graph = builder.build();
	}
};
