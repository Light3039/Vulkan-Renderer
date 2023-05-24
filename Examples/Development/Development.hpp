#pragma once

#include "Framework/Core/Application.hpp"
//
//
#include "BindlessVk/Context/VkContext.hpp"
#include "BindlessVk/Renderer/RenderNode.hpp"
#include "BindlessVk/Renderer/Renderer.hpp"
#include "BindlessVk/Renderer/Rendergraph.hpp"
#include "Framework/Common/Common.hpp"
#include "Framework/Core/Window.hpp"
#include "Framework/Scene/CameraController.hpp"
#include "Framework/Scene/Scene.hpp"
#include "Framework/Utils/Timer.hpp"
#include "Rendergraphs/Graph.hpp"
#include "Rendergraphs/Passes/Forward.hpp"
#include "Rendergraphs/Passes/UserInterface.hpp"

#include <imgui.h>

// @todo: load stuff from files
class DevelopmentExampleApplication: public Application
{
public:
	DevelopmentExampleApplication();
	~DevelopmentExampleApplication();

	void on_tick(f64 delta_time) final;

	void on_swapchain_recreate() final
	{
		assert_fail("Swapchain recreation not supported (yet)");
	}

private:
	BasicRendergraph::UserData graph_user_data {};
	Forwardpass::UserData fowardpass_user_data = {};

	BasicRendergraph render_graph = {};
	Forwardpass forward_pass = {};
	UserInterfacePass user_interface_pass = {};

private:
	void load_shaders();

	void load_graphics_pipelines();
	void load_compute_pipelines();

	void load_pipeline_configuration();

	void load_materials();

	void load_models();

	void load_entities();

	auto create_render_graph_blueprint() -> bvk::RenderNodeBlueprint;

	auto create_forward_pass_blueprint() -> bvk::RenderNodeBlueprint;

	auto create_ui_pass_blueprint() -> bvk::RenderNodeBlueprint;

	void initialize_render_nodes();

	void create_render_graph();
};
