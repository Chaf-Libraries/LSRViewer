#pragma once

#include <VulkanTools.h>
#include <VulkanDevice.h>

#include <scene/components/image.h>

namespace chaf
{
	class Texture
	{
	public:
		struct Mipmap
		{
			uint32_t level = 0;
			uint32_t offset = 0;
			VkExtent3D extent{ 0,0,0 };
		};

	public:
		void updateDescriptor();

		void destory();

		std::unique_ptr<Image> load(const std::string& filename, vks::VulkanDevice* device);

	public:
		vks::VulkanDevice* device;
		VkImage image;
		VkImageLayout image_layout;
		VkDeviceMemory device_memory;
		VkImageView view;
		uint32_t width, height, depth;
		uint32_t mip_level;
		uint32_t layer_count;
		VkDescriptorImageInfo descriptor;
		VkSampler sampler;
	};

	class Texture2D :public Texture
	{
	public:
		void loadFromFile(
			const std::string& filename,
			vks::VulkanDevice* device,
			VkQueue copy_queue,
			VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			bool force_linear = false);

		void loadFromBuffer(
			void* buffer,
			VkDeviceSize bufferSize,
			VkFormat format,
			uint32_t texWidth,
			uint32_t texHeight,
			vks::VulkanDevice* device,
			VkQueue copyQueue,
			VkFilter filter = VK_FILTER_LINEAR,
			VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	};

	// TODO: Cube Map

	// TODO: Texture Array
}
