/* Copyright (c) 2019-2020, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <framework/common/error.h>
#include <framework/common/utils.h>
#include <framework/common/vk_common.h>
#include <framework/core/instance.h>
#include <framework/platform/application.h>
#include <framework/rendering/render_context.h>
#include <framework/rendering/render_pipeline.h>
#include <framework/scene_graph/node.h>
#include <framework/scene_graph/scene.h>
#include <framework/scene_graph/scripts/node_animation.h>
#include <framework/stats/stats.h>

#include <ctpl_stl.h>

#include <ui/gui.h>

namespace chaf
{
	class AppBase : public vkb::Application
	{
	public:
		AppBase() = default;

		virtual ~AppBase();

		virtual bool prepare(vkb::Platform& platform) override;

		virtual void update(float delta_time) override;

		virtual void resize(const uint32_t width, const uint32_t height) override;

		virtual void input_event(const vkb::InputEvent& input_event) override;

		virtual void finish() override;

		std::unique_ptr<vkb::sg::Scene> load_scene(const std::string& path, int scene_index = -1);

		VkSurfaceKHR get_surface();

		vkb::Device& get_device();

		vkb::RenderContext& get_render_context();

		void set_render_pipeline(vkb::RenderPipeline&& render_pipeline);

		vkb::RenderPipeline& get_render_pipeline();

		vkb::Configuration& get_configuration();

		vkb::sg::Scene& get_scene();

	protected:

		std::unique_ptr<vkb::Instance> instance{ nullptr };

		std::unique_ptr<vkb::Device> device{ nullptr };

		std::unique_ptr<vkb::RenderContext> render_context{ nullptr };

		std::unique_ptr<vkb::RenderPipeline> render_pipeline{ nullptr };

		std::unique_ptr<vkb::sg::Scene> scene{ nullptr };

		std::unique_ptr<Gui> gui{ nullptr };

		std::unique_ptr<vkb::Stats> stats{ nullptr };

		std::mutex scene_mutex;

		std::mutex command_buffer_mutex;

	protected:

		std::vector<std::function<void(vkb::CommandBuffer&)>> secondary_commands;

		ctpl::thread_pool thread_pool;

		vkb::CommandBuffer* main_command_buffer;

		VkSemaphore acquired_semaphore;

		bool dirty{ false };

		bool loading{ false };

		size_t thread_num{ 5 };

	protected:

		void update_scene(float delta_time);

		void update_stats(float delta_time);

		void update_gui(float delta_time);

		virtual void draw(vkb::CommandBuffer& command_buffer, vkb::RenderTarget& render_target);

		virtual void draw_renderpass(vkb::CommandBuffer& command_buffer, vkb::RenderTarget& render_target);

		virtual void render(vkb::CommandBuffer& command_buffer);

		virtual const std::vector<const char*> get_validation_layers();

		const std::unordered_map<const char*, bool> get_instance_extensions();

		const std::unordered_map<const char*, bool> get_device_extensions();

		void add_device_extension(const char* extension, bool optional = false);

		void add_instance_extension(const char* extension, bool optional = false);

		void set_api_version(uint32_t requested_api_version);

		virtual void request_gpu_features(vkb::PhysicalDevice& gpu);

		virtual void prepare_render_context();

		virtual void reset_stats_view() {};

		virtual void draw_gui();

		virtual void update_debug_window();

		void set_viewport_and_scissor(vkb::CommandBuffer& command_buffer, const VkExtent2D& extent) const;

		static constexpr float STATS_VIEW_RESET_TIME{ 10.0f };        // 10 seconds

	protected:

		VkSurfaceKHR surface{ VK_NULL_HANDLE };

		vkb::Configuration configuration{};

	private:

		std::unordered_map<const char*, bool> device_extensions;

		std::unordered_map<const char*, bool> instance_extensions;

		uint32_t api_version = VK_API_VERSION_1_0;
	};
}
