#pragma once

#include "BindlessVk/Device.hpp"
#include "BindlessVk/RenderGraph.hpp"
#include "BindlessVk/RenderPass.hpp"
#include "BindlessVk/Renderer.hpp"
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
		load_shader_effects();

		load_pipeline_configuration();
		load_shader_passes();

		load_materials();

		load_models();
		load_entities();
		create_render_graph();
	}

	~DevelopmentExampleApplication()
	{
	}

	virtual void on_tick(double deltaTime) override
	{
		renderer.begin_frame(&scene);

		if (renderer.is_swapchain_invalidated()) {
			device_system.update_surface_info();
			renderer.on_swapchain_invalidated();
			camera_controller.on_window_resize(
			  window.get_framebuffer_size().width,
			  window.get_framebuffer_size().height
			);

			load_materials();
		}

		ImGui::ShowDemoWindow();
		CVar::draw_imgui_editor();

		camera_controller.update();
		renderer.end_frame(&scene);
	}

	virtual void on_swapchain_recreate() override
	{
		assert_fail("Swapchain recreation not supported (yet)");
	}

private:
	void load_shaders()
	{
		c_str DIRECTORY          = "Shaders/";
		c_str VERTEX_EXTENSION   = ".spv_vs";
		c_str FRAGMENT_EXTENSION = ".spv_fs";

		for (auto& shader : std::filesystem::directory_iterator(DIRECTORY)) {
			const str path(shader.path().c_str());
			const str extension(shader.path().extension().c_str());
			const str name(shader.path().filename().replace_extension().c_str());

			if (strcmp(extension.c_str(), VERTEX_EXTENSION) && strcmp(extension.c_str(), FRAGMENT_EXTENSION)) {
				continue;
			}

			shaders[hash_str(name.c_str())] = material_system.load_shader(
			  name.c_str(),
			  path.c_str(),
			  !strcmp(extension.c_str(), VERTEX_EXTENSION) ? vk::ShaderStageFlagBits::eVertex :
			                                                 vk::ShaderStageFlagBits::eFragment
			);
		}
	}

	void load_shader_effects()
	{
		shader_effects[hash_str("opaque_mesh")] = material_system.create_shader_effect(
		  "opaque_mesh",
		  {
		    &shaders[hash_str("defaultVertex")],
		    &shaders[hash_str("defaultFragment")],
		  }
		);

		shader_effects[hash_str("skybox")] = material_system.create_shader_effect(
		  "opaque_mesh",
		  {
		    &shaders[hash_str("skybox_vertex")],
		    &shaders[hash_str("skybox_fragment")],
		  }
		);
	}

	void load_pipeline_configuration()
	{
		bvk::Device* device = device_system.get_device();

		pipeline_configurations[hash_str("opaque_mesh"
		)] = material_system
		       .create_pipeline_configuration(
		         "opaque_mesh",
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
		           device->max_samples,
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
		         std::vector<vk::PipelineColorBlendAttachmentState> {
		           { VK_FALSE,
		             vk::BlendFactor::eSrcAlpha,
		             vk::BlendFactor::eOneMinusSrcAlpha,
		             vk::BlendOp::eAdd,
		             vk::BlendFactor::eOne,
		             vk::BlendFactor::eZero,
		             vk::BlendOp::eAdd,
		             vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
		               | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA },
		         },
		         vk::PipelineColorBlendStateCreateInfo {},
		         std::vector<vk::DynamicState> {
		           vk::DynamicState::eViewport,
		           vk::DynamicState::eScissor,
		         }
		       );

		pipeline_configurations[hash_str("skybox")] = material_system.create_pipeline_configuration(
		  "skybox",
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
		    device->max_samples,
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
		        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA // colorWriteMask
		    },
		  },
		  vk::PipelineColorBlendStateCreateInfo {},
		  std::vector<vk::DynamicState> {
		    vk::DynamicState::eViewport,
		    vk::DynamicState::eScissor,
		  }
		);
	}

	void load_shader_passes()
	{
		bvk::Device* device = device_system.get_device();

		shader_passes[hash_str("opaque_mesh")] = material_system.create_shader_pass(
		  "opaque_mesh",
		  &shader_effects[hash_str("opaque_mesh")],
		  device->surface_format.format,
		  device->depth_format,
		  pipeline_configurations[hash_str("opaque_mesh")]
		);

		shader_passes[hash_str("skybox")] = material_system.create_shader_pass(
		  "skybox",
		  &shader_effects[hash_str("skybox")],
		  device->surface_format.format,
		  device->depth_format,
		  pipeline_configurations[hash_str("skybox")]
		);
	}

	void load_materials()
	{
		materials[hash_str("opaque_mesh")] = material_system.create_material(
		  "opaque_mesh",
		  &shader_passes[hash_str("opaque_mesh")],
		  {}
		);

		materials[hash_str("skybox")] = material_system.create_material(
		  "skybox",
		  &shader_passes[hash_str("skybox")],
		  {}
		);
	}

	void load_models()
	{
		models[bvk::hash_str("flight_helmet")] = model_loader.load_from_gltf_ascii(
		  "flight_helmet",
		  "Assets/FlightHelmet/FlightHelmet.gltf",
		  staging_pool.get_by_index(0u),
		  staging_pool.get_by_index(1u),
		  staging_pool.get_by_index(2u)
		);

		models[bvk::hash_str("skybox")] = model_loader.load_from_gltf_ascii(
		  "skybox",
		  "Assets/Cube/Cube.gltf",
		  staging_pool.get_by_index(0u),
		  staging_pool.get_by_index(1u),
		  staging_pool.get_by_index(2u)
		);
	}

	// @todo: Load from files instead of hard-coding
	void load_entities()
	{
		Entity testModel = scene.create();

		scene.emplace<TransformComponent>(
		  testModel,
		  glm::vec3(0.0f),
		  glm::vec3(1.0f),
		  glm::vec3(0.0f, 0.0, 0.0)
		);

		scene.emplace<StaticMeshRendererComponent>(
		  testModel,
		  &materials[hash_str("opaque_mesh")],
		  &models[bvk::hash_str("flight_helmet")]
		);

		Entity skybox = scene.create();
		scene.emplace<TransformComponent>(
		  skybox,
		  glm::vec3(0.0f),
		  glm::vec3(1.0f),
		  glm::vec3(0.0f, 0.0, 0.0)
		);

		scene.emplace<StaticMeshRendererComponent>(
		  skybox,
		  &materials[hash_str("skybox")],
		  &models[bvk::hash_str("skybox")]
		);

		Entity light = scene.create();
		scene.emplace<TransformComponent>(
		  light,
		  glm::vec3(2.0f, 2.0f, 1.0f),
		  glm::vec3(1.0f),
		  glm::vec3(0.0f, 0.0, 0.0)
		);

		Entity camera = scene.create();
		scene.emplace<TransformComponent>(
		  camera,
		  glm::vec3(6.0, 7.0, 2.5),
		  glm::vec3(1.0),
		  glm::vec3(0.0f, 0.0, 0.0)
		);

		scene.emplace<CameraComponent>(
		  camera,
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

		scene.emplace<LightComponent>(light, 12);
	}

	void create_render_graph()
	{
		auto* device            = device_system.get_device();
		const auto color_format = device->surface_format.format;
		const auto depth_format = device->depth_format;
		const auto sample_count = device->max_samples;

		auto* default_texture      = &textures[hash_str("default_2d")];
		auto* default_texture_cube = &textures[hash_str("default_cube")];

		const std::array<float, 4> update_color  = { 1.0, 0.8, 0.8, 1.0 };
		const std::array<float, 4> barrier_color = { 0.8, 1.0, 0.8, 1.0 };
		const std::array<float, 4> render_color  = { 0.8, 0.8, 1.0, 1.0 };

		bvk::Renderpass::CreateInfo forward_pass_info {
                 "forwardpass",
                     {},
                 &forward_pass_update,
                 &forward_pass_render,
                 {
                    bvk::Renderpass::CreateInfo::AttachmentInfo {
                        .name = "forwardpass",
                        .size = { 1.0, 1.0 },
                        .size_type =
                            bvk::Renderpass::CreateInfo::SizeType::eSwapchainRelative,
                        .format  = color_format,
                        .samples = sample_count,
                        .clear_value =
                            vk::ClearValue{
                                vk::ClearColorValue {
                                    std::array<float, 4>({ 0.3f, 0.5f, 0.8f, 1.0f }) },
                            },
                    },
                },

                bvk::Renderpass::CreateInfo::AttachmentInfo {
                    .name       = "forward_depth",
                    .size       = { 1.0, 1.0 },
                    .size_type   = bvk::Renderpass::CreateInfo::SizeType::eSwapchainRelative,
                    .format     = depth_format,
                    .samples    = sample_count,
                    .clear_value = vk::ClearValue {
                        vk::ClearDepthStencilValue { 1.0, 0 },
                    },
                },

                 {
                    bvk::Renderpass::CreateInfo::TextureInputInfo {
                        .name           = "texture_2ds",
                        .binding        = 0,
                        .count          = 32,
                        .type           = vk::DescriptorType::eCombinedImageSampler,
                        .stage_mask      = vk::ShaderStageFlagBits::eFragment,
                        .default_texture = default_texture,
                    },
                    bvk::Renderpass::CreateInfo::TextureInputInfo {
                        .name           = "texture_cubes",
                        .binding        = 1,
                        .count          = 8u,
                        .type           = vk::DescriptorType::eCombinedImageSampler,
                        .stage_mask      = vk::ShaderStageFlagBits::eFragment,
                        .default_texture =default_texture_cube,
                    },
                },
                {},
                {},
               vk::DebugUtilsLabelEXT({
                        "forwardpass_update",
                        update_color,
                }),

               vk::DebugUtilsLabelEXT({
                        "forwardpass_barrier",
                        barrier_color,
                }),
               vk::DebugUtilsLabelEXT({
                        "forwardpass_render",
                        render_color,
                }),
            };

		bvk::Renderpass::CreateInfo ui_pass_info {
			"uipass",
			&user_interface_pass_begin_frame,
			&user_interface_pass_update,
			&user_interface_pass_render,
			{
			  bvk::Renderpass::CreateInfo::AttachmentInfo {
			    .name      = "backbuffer",
			    .size      = { 1.0, 1.0 },
			    .size_type = bvk::Renderpass::CreateInfo::SizeType::eSwapchainRelative,
			    .format    = color_format,
			    .samples   = sample_count,
			    .input     = "forwardpass",
			  },
			},
			{},
			{},
			{},
			{},
			vk::DebugUtilsLabelEXT({
			  "uipass_update",
			  update_color,
			}),

			vk::DebugUtilsLabelEXT({
			  "uipass_barrier",
			  barrier_color,
			}),
			vk::DebugUtilsLabelEXT({
			  "uipass_render",
			  render_color,
			}),
		};

		renderer.build_render_graph(
		  "backbuffer",
		  {
		    bvk::Renderpass::CreateInfo::BufferInputInfo {
		      .name       = "frame_data",
		      .binding    = 0,
		      .count      = 1,
		      .type       = vk::DescriptorType::eUniformBuffer,
		      .stage_mask = vk::ShaderStageFlagBits::eVertex,

		      .size         = (sizeof(glm::mat4) * 2) + (sizeof(glm::vec4)),
		      .initial_data = nullptr,
		    },
		    bvk::Renderpass::CreateInfo::BufferInputInfo {
		      .name       = "scene_data",
		      .binding    = 1,
		      .count      = 1,
		      .type       = vk::DescriptorType::eUniformBuffer,
		      .stage_mask = vk::ShaderStageFlagBits::eVertex,

		      .size         = sizeof(glm::vec4),
		      .initial_data = nullptr,
		    },
		  },
		  {
		    forward_pass_info,
		    ui_pass_info,
		  },
		  &render_graph_update,
		  nullptr,
		  vk::DebugUtilsLabelEXT({
		    "graph_update",
		    update_color,
		  }),
		  vk::DebugUtilsLabelEXT({
		    "backbuffer_present_barrier",
		    barrier_color,
		  })
		);
	}
};
