#pragma once

#include <VulkanDevice.h>

namespace chaf
{
	struct VirtualTexturePage
	{
		uint32_t index;
		uint32_t mip_level;
		uint32_t layer;
		VkExtent3D extent;
		VkOffset3D offset;
		VkDeviceSize size;
		VkSparseImageMemoryBind image_memory_bind;
	};

	class VirtualTexture
	{
	public:


	private:
		std::vector<VirtualTexturePage> pages;

		//std::unordered_map<uint32_t, 
	};
}