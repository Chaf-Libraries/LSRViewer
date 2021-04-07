#pragma once

#include <ctpl_stl.h>
#include <asset/lru.h>

#include <scene_graph/components/texture.h>

namespace chaf
{
	class AssetManager
	{
	public:
		AssetManager();

		ctpl::thread_pool& getThreadPool();



	private:
		ctpl::thread_pool thread_pool;


	};
}