#pragma once

#include <scene/cacher/cacher.h>
#include <scene/cacher/lru.h>
#include <scene/components/primitive.h>

#include <VulkanDevice.h>
#include <VulkanTools.h>

namespace chaf
{
	class BufferCacher: public Cacher
	{
	public:
		BufferCacher(vks::VulkanDevice& device, VkQueue& queue);

		virtual ~BufferCacher();

		bool hasVBO(uint32_t key);

		bool hasEBO(uint32_t key);

		VertexBuffer& getVBO(uint32_t key);

		IndexBuffer& getEBO(uint32_t key);

		bool isBusy() const;

		template<typename VBO_Ty, typename EBO_Ty>
		void addBuffer(uint32_t key, std::vector<VBO_Ty>& vertex_buffer_data, std::vector<EBO_Ty>& index_buffer_data);

	private:
		vks::VulkanDevice& device;
		
		VkQueue& queue;

		LruCacher<uint32_t, VertexBuffer, std::mutex> vbo_cache;

		LruCacher<uint32_t, IndexBuffer, std::mutex> ebo_cache;

		uint32_t num_task{ 0 };

	public:
		bool updated;
	};

	template<typename VBO_Ty, typename EBO_Ty>
	inline void BufferCacher::addBuffer(uint32_t key, std::vector<VBO_Ty>& vertex_buffer_data, std::vector<EBO_Ty>& index_buffer_data)
	{
		num_task++;
		Cacher::getThreadPool().push([this, key, &vertex_buffer_data, &index_buffer_data](size_t index) {

			// Target buffer
			auto vertex_buffer = std::make_unique<VertexBuffer>();
			auto index_buffer = std::make_unique<IndexBuffer>();

			size_t vertex_buffer_size = vertex_buffer_data.size() * sizeof(VBO_Ty);
			size_t index_buffer_size = index_buffer_data.size() * sizeof(EBO_Ty);

			struct StagingBuffer
			{
				VkBuffer buffer;
				VkDeviceMemory memory;
			} vertex_staging, index_staging;

			// Create host visible staging buffers (source)
			VK_CHECK_RESULT(device.createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				vertex_buffer_size,
				&vertex_staging.buffer,
				&vertex_staging.memory,
				vertex_buffer_data.data()));
			// Index data
			VK_CHECK_RESULT(device.createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				index_buffer_size,
				&index_staging.buffer,
				&index_staging.memory,
				index_buffer_data.data()));

			// Create device local buffers (target)
			VK_CHECK_RESULT(device.createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vertex_buffer_size,
				&vertex_buffer->buffer,
				&vertex_buffer->memory));

			VK_CHECK_RESULT(device.createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				index_buffer_size,
				&index_buffer->buffer,
				&index_buffer->memory));

			// Transfer
			VkCommandBuffer copyCmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy    copyRegion = {};

			copyRegion.size = vertex_buffer_size;
			vkCmdCopyBuffer(
				copyCmd,
				vertex_staging.buffer,
				vertex_buffer->buffer,
				1,
				&copyRegion);

			copyRegion.size = index_buffer_size;
			vkCmdCopyBuffer(
				copyCmd,
				index_staging.buffer,
				index_buffer->buffer,
				1,
				&copyRegion);

			device.flushCommandBuffer(copyCmd, queue, true);

			// Free staging resources
			vkDestroyBuffer(device.logicalDevice, vertex_staging.buffer, nullptr);
			vkFreeMemory(device.logicalDevice, vertex_staging.memory, nullptr);
			vkDestroyBuffer(device.logicalDevice, index_staging.buffer, nullptr);
			vkFreeMemory(device.logicalDevice, index_staging.memory, nullptr);

			vbo_cache.insert(key, std::move(vertex_buffer));
			ebo_cache.insert(key, std::move(index_buffer));
			
			this->num_task--;
			updated = true; });
	}

}