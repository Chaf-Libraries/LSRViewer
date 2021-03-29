#pragma once

#include <framework/scene_graph/scene.h>

namespace chaf
{
	class Hierarchy
	{
	public:
		static void draw(vkb::sg::Scene* scene);
		static void reset_selected();
		static vkb::sg::Node* get_selected();
	private:
		static void draw_node(vkb::sg::Node* node, uint32_t& unname);
	private:
		static vkb::sg::Node* selected_node;
		static vkb::sg::Node* root;
		static uint32_t vertices_count;
		static uint32_t triangle_count;
	};
}