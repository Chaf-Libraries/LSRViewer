#pragma once

#include <VulkanTools.h>

namespace chaf
{
	class VirtualTexturePage
	{
	public:
		VirtualTexturePage();

		bool resident();

		void allocate(VkDevice device, uint32_t memory_type_index);

		void release(VkDevice device);

	public:
		VkExtent3D extent;
		VkOffset3D offset;
		VkSparseImageMemoryBind image_memory_bind;
		VkDeviceSize size;
		uint32_t mip_level;
		uint32_t layer;
		uint32_t index;
	};

	class VirtualTexture
	{
	public:
		VirtualTexturePage* addPage(VkOffset3D offset, VkExtent3D extent, const VkDeviceSize size, const uint32_t mip_level, uint32_t layer);

		void updateSparseBindInfo();

		void destroy();

	public:
		VkDevice device;
		VkImage image;
		VkBindSparseInfo bind_sparse_info;
		std::vector<VirtualTexturePage> pages;
		std::vector<VkSparseImageMemoryBind> sparse_image_memory_binds;
		std::vector<VkSparseMemoryBind> sparse_memory_binds;
		VkSparseImageMemoryBindInfo image_memory_bind_info;
		VkSparseImageOpaqueMemoryBindInfo opaque_memory_bind_info;
		uint32_t mip_tail_start;
		VkSparseImageMemoryRequirements sparse_image_memory_requirements;
		uint32_t memory_type_index;

		struct MipTailInfo
		{
			bool single_mip_tail;
			bool aling_mip_size;
		}mip_tail_info;
	};
}