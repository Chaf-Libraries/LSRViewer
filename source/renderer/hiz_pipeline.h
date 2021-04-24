#pragma once

#include <renderer/base_pipeline.h>

class ScenePipeline;

class HizPipeline :public chaf::PipelineBase
{
public:
	HizPipeline(vks::VulkanDevice& device, ScenePipeline& scene_pipeline);

	~HizPipeline();

	void prepareHiz();

	void resize();

	void prepare(VkPipelineCache& pipeline_cache);

	void buildCommandBuffer();

	void submit();


public:
	ScenePipeline& scene_pipeline;

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

	// Save last depth image: copy from depth stencil attachment

	struct
	{
		uint32_t depth_pyramid_levels{ 1 };
		VkSampler sampler{ VK_NULL_HANDLE };
		VkDeviceMemory mem{ VK_NULL_HANDLE };
		VkImage image{ VK_NULL_HANDLE };
		std::vector<VkImageView> views;
		VkDescriptorImageInfo descriptor;
	}hiz_image;


};
