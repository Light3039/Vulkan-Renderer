#pragma once

#include "Framework/Common/Common.hpp"
//

#include "BindlessVk/MaterialSystem.hpp"
#include "BindlessVk/ModelLoader.hpp"
#include "BindlessVk/Renderer.hpp"
#include "BindlessVk/Shader.hpp"
#include "BindlessVk/ShaderLoader.hpp"
#include "BindlessVk/Texture.hpp"
#include "Framework/Core/Window.hpp"
#include "Framework/Pools/StagingPool.hpp"
#include "Framework/Scene/CameraController.hpp"
#include "Framework/Scene/Scene.hpp"
#include "Framework/Utils/Logger.hpp"

#include <GLFW/glfw3.h>

class Application
{
public:
	Application();
	virtual ~Application();

	virtual void on_tick(double delta_time) = 0;
	virtual void on_swapchain_recreate() = 0;

public:
	Logger logger = {};

	Scene scene = {};
	Window window = {};
	StagingPool staging_pool = {};
	CameraController camera_controller = {};

	ref<bvk::VkContext> vk_context = {};
	scope<bvk::Renderer> renderer = {};
	bvk::RenderGraph render_graph = {};

	bvk::TextureLoader texture_loader = {};
	bvk::ModelLoader model_loader = {};
	bvk::ShaderLoader shader_loader = {};

	hash_map<u64, bvk::Model> models = {};
	hash_map<u64, bvk::Texture> textures = {};
	hash_map<u64, bvk::Shader> shaders = {};
	hash_map<u64, bvk::ShaderEffect> shader_effects = {};
	hash_map<u64, bvk::ShaderEffect::Configuration> shader_effect_configurations = {};
	hash_map<u64, bvk::Material> materials = {};

	vk::DescriptorPool descriptor_pool = {};

private:
	void create_window();
	void create_vk_context();
	void create_descriptor_pool();
	void create_user_interface();
	void create_loaders();
	void create_renderer();

	void load_default_textures();

	auto get_layers() const -> vec<c_str>;
	auto get_instance_extensions() const -> vec<c_str>;
	auto get_device_extensions() const -> vec<c_str>;
	auto get_physical_device_features() const -> vk::PhysicalDeviceFeatures;

	void destroy_models();
	void destroy_user_interface();
	void destroy_descriptor_pool();
};
