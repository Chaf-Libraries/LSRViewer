#include "app_base.h"

#include "common/error.h"

VKBP_DISABLE_WARNINGS()
#include "common/glm_common.h"
#include <imgui.h>
VKBP_ENABLE_WARNINGS()

#include "common/helpers.h"
#include "common/logging.h"
#include "common/strings.h"
#include "common/utils.h"
#include "common/vk_common.h"
#include "gltf_loader.h"
#include "platform/platform.h"
#include "platform/window.h"
#include "scene_graph/components/camera.h"
#include "scene_graph/script.h"
#include "scene_graph/scripts/free_camera.h"

using namespace vkb;

namespace chaf
{
	AppBase::~AppBase()
	{
		if (device)
		{
			device->wait_idle();
		}
		
		scene_mutex.lock();

		scene.reset();

		scene_mutex.unlock();

		stats.reset();
		gui.reset();
		render_context.reset();
		device.reset();

		if (surface != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(instance->get_handle(), surface, nullptr);
		}

		instance.reset();
	}

	void AppBase::set_render_pipeline(RenderPipeline&& rp)
	{
		render_pipeline = std::make_unique<RenderPipeline>(std::move(rp));
	}

	RenderPipeline& AppBase::get_render_pipeline()
	{
		assert(render_pipeline && "Render pipeline was not created");
		return *render_pipeline;
	}

	bool AppBase::prepare(Platform& platform)
	{
		if (!Application::prepare(platform))
		{
			return false;
		}

		LOGI("Initializing Vulkan sample");

		// Creating the vulkan instance
		add_instance_extension(platform.get_surface_extension());
		instance = std::make_unique<Instance>(get_name(), get_instance_extensions(), get_validation_layers(), is_headless(), api_version);

		// Getting a valid vulkan surface from the platform
		surface = platform.get_window().create_surface(*instance);

		auto& gpu = instance->get_suitable_gpu();

		// Request to enable ASTC
		if (gpu.get_features().textureCompressionASTC_LDR)
		{
			gpu.get_mutable_requested_features().textureCompressionASTC_LDR = VK_TRUE;
		}

		// Request sample required GPU features
		request_gpu_features(gpu);

		// Creating vulkan device, specifying the swapchain extension always
		if (!is_headless() || instance->is_enabled(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME))
		{
			add_device_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		device = std::make_unique<vkb::Device>(gpu, surface, get_device_extensions());

		// Preparing render context for rendering
		render_context = std::make_unique<vkb::RenderContext>(*device, surface, platform.get_window().get_width(), platform.get_window().get_height());
		render_context->set_present_mode_priority({ VK_PRESENT_MODE_MAILBOX_KHR,
																					VK_PRESENT_MODE_FIFO_KHR });

		render_context->request_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);

		render_context->set_surface_format_priority({ {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
													 {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
													 {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
													 {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} });

		prepare_render_context();

		stats = std::make_unique<vkb::Stats>(*render_context);

		thread_pool.resize(std::thread::hardware_concurrency());


		return true;
	}

	void AppBase::prepare_render_context()
	{
		render_context->prepare(thread_num);
	}

	void AppBase::update_scene(float delta_time)
	{
		scene_mutex.lock();
		if (scene)
		{
			//Update scripts
			if (scene->has_component<sg::Script>())
			{
				auto scripts = scene->get_components<sg::Script>();

				for (auto script : scripts)
				{
					script->update(delta_time);
				}
			}
		}
		scene_mutex.unlock();
	}

	void AppBase::update_stats(float delta_time)
	{
		if (stats)
		{
			stats->update(delta_time);

			static float stats_view_count = 0.0f;
			stats_view_count += delta_time;

			// Reset every STATS_VIEW_RESET_TIME seconds
			if (stats_view_count > STATS_VIEW_RESET_TIME)
			{
				reset_stats_view();
				stats_view_count = 0.0f;
			}
		}
	}

	void AppBase::update_gui(float delta_time)
	{
		if (gui)
		{
			if (gui->is_debug_view_active())
			{
				update_debug_window();
			}

			gui->new_frame();

			gui->show_top_window(get_name(), stats.get(), &get_debug_info());

			// Samples can override this
			draw_gui();

			gui->update(delta_time);
		}
	}

	void AppBase::update(float delta_time)
	{
		/*auto scene_future = thread_pool.push([this, &delta_time](size_t thread_index) { });
		auto gui_future = thread_pool.push([this, &delta_time](size_t thread_index) { });*/
		update_gui(delta_time);
		update_scene(delta_time);
		/*scene_future.wait();
		gui_future.wait();*/
		update_stats(delta_time);
		//update_gui(delta_time);

		//update_stats(delta_time);

		command_buffer_mutex.lock();

		auto& command_buffer = render_context->begin();

		

		command_buffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		stats->begin_sampling(command_buffer);

		draw(command_buffer, render_context->get_active_frame().get_render_target());

		stats->end_sampling(command_buffer);
		command_buffer.end();

		render_context->submit(command_buffer);

		command_buffer_mutex.unlock();
	}

	void AppBase::draw(CommandBuffer& command_buffer, RenderTarget& render_target)
	{
		auto& views = render_target.get_views();

		{
			// Image 0 is the swapchain
			ImageMemoryBarrier memory_barrier{};
			memory_barrier.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			memory_barrier.new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			memory_barrier.src_access_mask = 0;
			memory_barrier.dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			memory_barrier.src_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			memory_barrier.dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			command_buffer.image_memory_barrier(views.at(0), memory_barrier);

			// Skip 1 as it is handled later as a depth-stencil attachment
			for (size_t i = 2; i < views.size(); ++i)
			{
				command_buffer.image_memory_barrier(views.at(i), memory_barrier);
			}
		}

		{
			ImageMemoryBarrier memory_barrier{};
			memory_barrier.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			memory_barrier.new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			memory_barrier.src_access_mask = 0;
			memory_barrier.dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			memory_barrier.src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			memory_barrier.dst_stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

			command_buffer.image_memory_barrier(views.at(1), memory_barrier);
		}

		draw_renderpass(command_buffer, render_target);

		{
			ImageMemoryBarrier memory_barrier{};
			memory_barrier.old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			memory_barrier.new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			memory_barrier.src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			memory_barrier.src_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			memory_barrier.dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

			command_buffer.image_memory_barrier(views.at(0), memory_barrier);
		}
	}

	void AppBase::draw_renderpass(CommandBuffer& command_buffer, RenderTarget& render_target)
	{
		if (secondary_commands.size() > thread_num)
		{
			throw std::runtime_error("Thread number out of range");
		}

		set_viewport_and_scissor(command_buffer, render_target.get_extent());

		render(command_buffer);

		std::vector<vkb::CommandBuffer*> secondary_command_buffers(secondary_commands.size() + 1);
		
		secondary_command_buffers[0] = &render_context->get_active_frame().request_command_buffer(device->get_queue_by_flags(VK_QUEUE_GRAPHICS_BIT, 0), vkb::CommandBuffer::ResetMode::ResetPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY, 0);
		
		auto wait_ui = thread_pool.push([this, &secondary_command_buffers, &command_buffer](size_t thread_id) {
			secondary_command_buffers[0]->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, &command_buffer);
			if (gui)
			{
				gui->draw(*secondary_command_buffers[0]);
			}
			secondary_command_buffers[0]->end();
			});

		std::vector<std::future<void>*> futures = { &wait_ui };
		
		for (size_t i = 0; i < secondary_commands.size(); i++)
		{
			secondary_command_buffers[i+1] = &render_context->get_active_frame().request_command_buffer(device->get_queue_by_flags(VK_QUEUE_GRAPHICS_BIT, 0), vkb::CommandBuffer::ResetMode::ResetPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY, i + 1);
			futures.push_back(&thread_pool.push([this, &secondary_command_buffers, &i, &command_buffer](size_t thread_id) {
				secondary_command_buffers[i + 1]->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, &command_buffer);
				secondary_commands[i](*secondary_command_buffers[i + 1]);
				secondary_command_buffers[i + 1]->end();
				}));
		}

		for (auto future : futures)
		{
			future->get();
		}

		command_buffer.execute_commands(secondary_command_buffers);

		command_buffer.end_render_pass();
	}

	void AppBase::render(CommandBuffer& command_buffer)
	{
		if (render_pipeline)
		{
			render_pipeline->draw(command_buffer, render_context->get_active_frame().get_render_target());
		}
	}

	void AppBase::resize(uint32_t width, uint32_t height)
	{
		Application::resize(width, height);

		if (gui)
		{
			gui->resize(width, height);
		}

		scene_mutex.lock();
		if (scene && scene->has_component<sg::Script>())
		{
			auto scripts = scene->get_components<sg::Script>();

			for (auto script : scripts)
			{
				script->resize(width, height);
			}
		}
		scene_mutex.unlock();

		if (stats)
		{
			stats->resize(width);
		}
	}

	void AppBase::input_event(const InputEvent& input_event)
	{
		Application::input_event(input_event);

		bool gui_captures_event = false;

		if (gui)
		{
			gui_captures_event = gui->input_event(input_event);
		}

		if (!gui_captures_event)
		{
			scene_mutex.lock();
			if (scene && scene->has_component<sg::Script>())
			{
				auto scripts = scene->get_components<sg::Script>();

				for (auto script : scripts)
				{
					script->input_event(input_event);
				}
			}
			scene_mutex.unlock();
		}

		if (input_event.get_source() == EventSource::Keyboard)
		{
			const auto& key_event = static_cast<const KeyInputEvent&>(input_event);
			if (key_event.get_action() == KeyAction::Down && key_event.get_code() == KeyCode::PrintScreen)
			{
				screenshot(*render_context, "screenshot-" + get_name());
			}

			if (key_event.get_code() == KeyCode::F6 && key_event.get_action() == KeyAction::Down)
			{
				scene_mutex.lock();
				if (!graphs::generate_all(get_render_context(), *scene.get()))
				{
					LOGE("Failed to save Graphs");
				}
				scene_mutex.unlock();
			}
		}
	}

	void AppBase::finish()
	{
		Application::finish();

		if (device)
		{
			device->wait_idle();
		}
	}

	Device& AppBase::get_device()
	{
		return *device;
	}

	Configuration& AppBase::get_configuration()
	{
		return configuration;
	}

	void AppBase::draw_gui()
	{
	}

	void AppBase::update_debug_window()
	{
		auto driver_version = device->get_driver_version();
		std::string driver_version_str = fmt::format("major: {} minor: {} patch: {}", driver_version.major, driver_version.minor, driver_version.patch);
		get_debug_info().insert<field::Static, std::string>("driver_version", driver_version_str);

		get_debug_info().insert<field::Static, std::string>("resolution",
			to_string(render_context->get_swapchain().get_extent()));

		get_debug_info().insert<field::Static, std::string>("surface_format",
			to_string(render_context->get_swapchain().get_format()) + " (" +
			to_string(get_bits_per_pixel(render_context->get_swapchain().get_format())) + "bpp)");

		scene_mutex.lock();
		get_debug_info().insert<field::Static, uint32_t>("mesh_count", to_u32(scene->get_components<sg::SubMesh>().size()));

		get_debug_info().insert<field::Static, uint32_t>("texture_count", to_u32(scene->get_components<sg::Texture>().size()));

		if (scene->has_component<vkb::sg::Camera>())
		{
			if (auto camera = scene->get_components<vkb::sg::Camera>().at(0))
			{
				if (auto camera_node = camera->get_node())
				{
					const glm::vec3& pos = camera_node->get_transform().get_translation();
					get_debug_info().insert<field::Vector, float>("camera_pos", pos.x, pos.y, pos.z);
				}
			}
		}

		scene_mutex.unlock();
	}

	void AppBase::set_viewport_and_scissor(vkb::CommandBuffer& command_buffer, const VkExtent2D& extent) const
	{
		VkViewport viewport{};
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		command_buffer.set_viewport(0, { viewport });

		VkRect2D scissor{};
		scissor.extent = extent;
		command_buffer.set_scissor(0, { scissor });
	}

	std::unique_ptr<vkb::sg::Scene> AppBase::load_scene(const std::string& path, int scene_index)
	{
		GLTFLoader loader{ *device };

		auto result = loader.read_scene_from_file(path, scene_index);

		if (!result)
		{
			LOGE("Cannot load scene: {}", path.c_str());
			throw std::runtime_error("Cannot load scene: " + path);
		}

		return result;
	}

	VkSurfaceKHR AppBase::get_surface()
	{
		return surface;
	}

	RenderContext& AppBase::get_render_context()
	{
		assert(render_context && "Render context is not valid");
		return *render_context;
	}

	const std::vector<const char*> AppBase::get_validation_layers()
	{
		return {};
	}

	const std::unordered_map<const char*, bool> AppBase::get_instance_extensions()
	{
		return instance_extensions;
	}

	const std::unordered_map<const char*, bool> AppBase::get_device_extensions()
	{
		return device_extensions;
	}

	void AppBase::add_device_extension(const char* extension, bool optional)
	{
		device_extensions[extension] = optional;
	}

	void AppBase::add_instance_extension(const char* extension, bool optional)
	{
		instance_extensions[extension] = optional;
	}

	void AppBase::set_api_version(uint32_t requested_api_version)
	{
		api_version = requested_api_version;
	}

	void AppBase::request_gpu_features(PhysicalDevice& gpu)
	{
		// To be overriden by sample
	}

	sg::Scene& AppBase::get_scene()
	{
		assert(scene && "Scene not loaded");
		return *scene;
	}

}        // namespace vkb
