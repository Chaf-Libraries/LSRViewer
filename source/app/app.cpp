#include <app/app.h>

#include <framework/rendering/subpasses/forward_subpass.h>
#include <framework/platform/platform.h>
#include <framework/scene_graph/components/perspective_camera.h>
#include <framework/scene_graph/components/camera.h>

#include <ui/hierarchy.h>
#include <ui/inspector.h>
#include <ui/loader.h>



namespace chaf
{
	App::App()
	{
		set_name("Large Scene Renderer");

		add_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		
		add_device_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		add_device_extension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		add_device_extension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
	}

	App::~App()
	{
		if (pipeline_cache != VK_NULL_HANDLE)
		{
			size_t size{};
			VK_CHECK(vkGetPipelineCacheData(device->get_handle(), pipeline_cache, &size, nullptr));

			std::vector<uint8_t> data(size);
			VK_CHECK(vkGetPipelineCacheData(device->get_handle(), pipeline_cache, &size, data.data()));

			vkb::fs::write_temp(data, "pipeline_cache.data");

			vkDestroyPipelineCache(device->get_handle(), pipeline_cache, nullptr);
		}

		vkb::fs::write_temp(device->get_resource_cache().serialize(), "cache.data");
	}

	bool App::prepare(vkb::Platform& platform)
	{
		if (!AppBase::prepare(platform))
		{
			return false;
		}

		std::vector<uint8_t> pipeline_data;

		try
		{
			pipeline_data = vkb::fs::read_temp("pipeline_cache.data");
		}
		catch(std::runtime_error &e)
		{
			LOGW("No pipeline cache found {}", e.what());
		}

		resource_manager = std::make_unique<ResourceManager>(*device);


		VkPipelineCacheCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
		create_info.initialDataSize = pipeline_data.size();
		create_info.pInitialData = pipeline_data.data();

		VK_CHECK(vkCreatePipelineCache(device->get_handle(), &create_info, nullptr, &pipeline_cache));

		device->get_resource_cache().set_pipeline_cache(pipeline_cache);

		std::vector<uint8_t> data_cache;

		try
		{
			data_cache = vkb::fs::read_temp("cache.data");
		}
		catch (std::runtime_error& ex)
		{
			LOGW("No data cache found. {}", ex.what());
		}

		device->get_resource_cache().warmup(data_cache);

		stats->request_stats({ vkb::StatIndex::frame_times });

		gui = std::make_unique<Gui>(get_render_context(), platform.get_window(), stats.get(), 15.f);

		//auto loading = thread_pool.push([this](size_t thread_id) { this->second_scene = this->load_scene("scenes/viking_room/scene.gltf"); });

		//loading.get();

		//auto& camera_node = vkb::add_free_camera(*scene, "main_camera", get_render_context().get_surface_extent());
		//camera = &camera_node.get_component<vkb::sg::Camera>();
		scene = std::make_unique<vkb::sg::Scene>("scene");
		auto root = std::make_unique<vkb::sg::Node>(0, "root");
		default_camera = std::make_unique<vkb::sg::PerspectiveCamera>("default_camera");
		root->set_component(*default_camera);
		default_camera->set_node(*root);
		scene->set_root_node(*root);
		scene->add_node(std::move(root));
		vkb::ShaderSource vert_shader("my_pbr.vert");
		vkb::ShaderSource frag_shader("my_pbr.frag");
		auto scene_subpass = std::make_unique<vkb::ForwardSubpass>(get_render_context(), std::move(vert_shader), std::move(frag_shader), *scene, *default_camera);

		auto render_pipeline = vkb::RenderPipeline();
		render_pipeline.add_subpass(std::move(scene_subpass));

		set_render_pipeline(std::move(render_pipeline));

		return true;
	}

	void App::draw_gui()
	{
		Hierarchy::draw(scene.get());
		Inspector::draw(Hierarchy::get_selected());
		Loader::draw(*resource_manager);

		ImGui::ShowDemoWindow();

		ImGui::Begin("test");
		if (ImGui::Button("Load1"))
		{

			resource_manager->require_new_scene("scenes/sponza/Sponza01.gltf");
			//if (!loading)
			//{
			//	auto loading_model = thread_pool.push([this](size_t thread_id) { this->second_scene = this->load_scene(, 1); this->dirty = true; });
			//	loading = true;
			//}
		}
		if (ImGui::Button("Load2"))
		{
			if (!loading)
			{
				auto loading_model = thread_pool.push([this](size_t thread_id) { this->second_scene = this->load_scene("scenes/the_noble_craftsman/scene.gltf", 1); this->dirty = true; });
				loading = true;
			}
		}
		if (ImGui::Button("Load3"))
		{
			if (!loading)
			{
				auto loading_model = thread_pool.push([this](size_t thread_id) { this->second_scene = this->load_scene("scenes/the_amazing_spiderman/scene.gltf", 1); this->dirty = true; });
				loading = true;
			}
		}
		ImGui::End();
	}

	void App::update(float delta_time)
	{
		resource_manager->update([this](std::unique_ptr<vkb::sg::Scene>& scene_cache) {
			Hierarchy::reset_selected();
			this->scene.reset();

			this->scene = std::unique_ptr<vkb::sg::Scene>(std::move(scene_cache));

			auto& camera_node = vkb::add_free_camera(*(this->scene), "main_camera", get_render_context().get_surface_extent());
			this->camera = &camera_node.get_component<vkb::sg::Camera>();

			//camera_node.get_component<vkb::sg::Transform>().set_translation({ 0.f, 0.f,5.f });

			//this->scene->get_root_node().get_component<vkb::sg::Transform>().set_translation({ 0.f,0.f,0.f });
			//this->scene->get_root_node().get_component<vkb::sg::Transform>().set_scale({ 10.f,10.f,10.f });

			vkb::ShaderSource vert_shader("pbr.vert");
			vkb::ShaderSource frag_shader("pbr.frag");
			auto scene_subpass = std::make_unique<vkb::ForwardSubpass>(get_render_context(), std::move(vert_shader), std::move(frag_shader), *(this->scene), *(this->camera));

			auto render_pipeline = vkb::RenderPipeline();
			render_pipeline.add_subpass(std::move(scene_subpass));

			set_render_pipeline(std::move(render_pipeline));
			});

		AppBase::update(delta_time);
	}
}