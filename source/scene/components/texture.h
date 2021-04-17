#pragma once

#include <VulkanTools.h>
#include <VulkanDevice.h>

#include <ktx.h>
#include <stb_image.h>

namespace chaf
{
	class Texture
	{
	public:
		void updateDescriptor();

		void destory();

		uint8_t* loadKTX(const std::string& filename, ktxTexture** target);

		bool loadStb(const std::string& filename, stbi_uc** target);

	public:
		vks::VulkanDevice& device;
		VkImage image;
		VkImageLayout image_layout;
		VkDeviceMemory device_memory;
		VkImageView view;
		uint32_t width, height;
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
			VkFormat format,
			vks::VulkanDevice* device,
			VkQueue copy_queue,
			VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			bool force_linear = false);

		void loadFromBuffer(
			void* buffer,
			VkDeviceSize buffer_size,
			VkFormat format,
			uint32_t tex_width,
			uint32_t tex_height,
			vks::VulkanDevice* device,
			VkQueue copy_queue,
			VkFilter filter = VK_FILTER_LINEAR,
			VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	};

	class Texture2DArray :public Texture
	{
		void loadFromFile(
			const std::string& filename,
			VkFormat format,
			vks::VulkanDevice* device,
			VkQueue copy_queue,
			VkImageUsageFlags image_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	};
}
