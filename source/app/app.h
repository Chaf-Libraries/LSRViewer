#pragma once

#include "app_base.h"
#include <framework/scene_graph/components/camera.h>

#include <resource/resource_manager.h>

#include <ctpl_stl.h>

namespace chaf
{
	class App :public AppBase
	{
	public:
		App();

		virtual ~App();

		virtual bool prepare(vkb::Platform& platform) override;

		virtual void update(float delta_time) override;

	private:
		vkb::sg::Camera* camera{ nullptr };
		std::unique_ptr<vkb::sg::Camera> default_camera{ nullptr };

		

		VkPipelineCache pipeline_cache{ VK_NULL_HANDLE };

		std::unique_ptr<vkb::sg::Scene> second_scene{ nullptr };

		virtual void draw_gui() override;

	private:
		std::unordered_map<const char*, vkb::RenderPipeline> render_pipelines;
		std::unique_ptr<ResourceManager> resource_manager;
	};
}