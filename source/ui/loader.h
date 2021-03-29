#pragma once

#include <framework/scene_graph/scene.h>
#include <framework/scene_graph/node.h>

#include <resource/resource_manager.h>

namespace chaf
{
	class Loader
	{
	public:
		static void draw(ResourceManager& resource_manager);
	};
}