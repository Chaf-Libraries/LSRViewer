#pragma once

#include <ctpl_stl.h>

namespace chaf
{
	class Cacher
	{
	public:
		Cacher() = default;
		
		virtual ~Cacher() = default;

		static ctpl::thread_pool& getThreadPool()
		{
			if (thread_pool.size() < 1)
			{
				thread_pool.resize(std::thread::hardware_concurrency());
			}

			return thread_pool;
		}

	private:
		static ctpl::thread_pool thread_pool;
	};
}