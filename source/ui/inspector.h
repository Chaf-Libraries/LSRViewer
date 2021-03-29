#pragma once

#include <framework/scene_graph/node.h>

namespace chaf
{
	class Inspector
	{
	public:
		static void draw(vkb::sg::Node* node);
	};
}