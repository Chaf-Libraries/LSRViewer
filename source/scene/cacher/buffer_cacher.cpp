#include <scene/cacher/buffer_cacher.h>

namespace chaf
{
	BufferCacher::BufferCacher(vks::VulkanDevice& device, VkQueue& queue) :
		device{ device },
		queue{ queue }
	{
		vbo_cache.setRelease([&device](VertexBuffer& vbo) {
			if (vbo.buffer != VK_NULL_HANDLE)
			{
				vkDestroyBuffer(device.logicalDevice, vbo.buffer, nullptr);
			}

			if (vbo.memory != VK_NULL_HANDLE)
			{
				vkFreeMemory(device.logicalDevice, vbo.memory, nullptr);
			}
			});

		ebo_cache.setRelease([&device](IndexBuffer& ebo) {
			if (ebo.buffer != VK_NULL_HANDLE)
			{
				vkDestroyBuffer(device.logicalDevice, ebo.buffer, nullptr);
			}

			if (ebo.memory != VK_NULL_HANDLE)
			{
				vkFreeMemory(device.logicalDevice, ebo.memory, nullptr);
			}
			});
	}

	BufferCacher::~BufferCacher()
	{
		vbo_cache.clear();
		ebo_cache.clear();
	}

	bool BufferCacher::vboExist(uint32_t key)
	{
		std::lock_guard g(mutex);
		return vbo_cache.contain(key);
	}

	bool BufferCacher::eboExist(uint32_t key)
	{
		std::lock_guard g(mutex);
		return ebo_cache.contain(key);
	}

	VertexBuffer& BufferCacher::getVBO(uint32_t key)
	{
		std::lock_guard g(mutex);
		return vbo_cache.get(key);
	}

	IndexBuffer& BufferCacher::getEBO(uint32_t key)
	{
		std::lock_guard g(mutex);
		return ebo_cache.get(key);
	}


}