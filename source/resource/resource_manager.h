#pragma once

#include <framework/scene_graph/scene.h>
#include <framework/scene_graph/node.h>

#include <ctpl_stl.h>

namespace chaf
{
	class ResourceManager
	{
	public:
		ResourceManager(vkb::Device& device);

		void update(std::function<void(std::unique_ptr<vkb::sg::Scene>&)> callback);

		void require_new_scene(const std::string& filepath);

		void require_new_model(const std::string& filepath);

	private:
		std::unique_ptr<vkb::sg::Scene> load_scene(const std::string& path, int scene_index);

	private:
		vkb::Device& device;
		std::unique_ptr<vkb::sg::Scene> scene_cache;
		bool is_dirty{ false };
		bool is_loading{ false };
		bool is_add_node{ false };
		ctpl::thread_pool thread_pool;
	};
}