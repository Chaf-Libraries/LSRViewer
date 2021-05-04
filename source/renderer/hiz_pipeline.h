#pragma once

#include <renderer/base_pipeline.h>

class HizPipeline :public chaf::PipelineBase
{
public:
	HizPipeline(vks::VulkanDevice& device, uint32_t width, uint32_t height);

	~HizPipeline();

	void resize(uint32_t width, uint32_t height, VkQueue queue);

	void prepare(VkQueue queue, VkFormat depth_format);

	void buildCommandBuffer();

	void submit();

public:
	VkQueue compute_queue;

	VkFence fence;

	VkSemaphore semaphore;

	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;

	VkDescriptorSetLayout descriptor_set_layout;

	std::vector<VkDescriptorSet> descriptor_sets;

	VkDescriptorPool descriptor_pool;

	VkCommandPool command_pool;

	VkCommandBuffer command_buffer;

	struct
	{
		uint32_t depth_pyramid_levels{ 1 };
		VkSampler sampler{ VK_NULL_HANDLE };
		VkDeviceMemory mem{ VK_NULL_HANDLE };
		VkImage image{ VK_NULL_HANDLE };
		std::vector<VkImageView> views;
		VkDescriptorImageInfo descriptor;
	}hiz_image;

	void prepareHiz();
	void destroyHiz();

	struct
	{
		VkSampler sampler{ VK_NULL_HANDLE };
		VkImageView view{ VK_NULL_HANDLE };
		VkImage image{ VK_NULL_HANDLE };
		VkDeviceMemory mem{ VK_NULL_HANDLE };
		VkDescriptorImageInfo descriptor;
		uint32_t width, height;
		VkFormat format;
	}depth_image;

	void prepareDepth(VkQueue queue, VkFormat depth_format);
	void destroyDepth();
	void copyDepth(VkCommandBuffer cmd_buffer, VkImage depth_stencil_image);
};
