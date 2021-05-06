#pragma once

// For visualize texture

#include <renderer/base_pipeline.h>
#include <renderer/hiz_pipeline.h>
#include <renderer/scene_pipeline.h>

class DebugPipeline :public chaf::PipelineBase
{
public:
	DebugPipeline(vks::VulkanDevice& device);

	~DebugPipeline();

	void buildCommandBuffer(VkCommandBuffer& cmd_buffer, VkRenderPassBeginInfo& renderPassBeginInfo);

	void setupDescriptors(HizPipeline& hiz_pipeline, vks::Buffer& uniform_buffer);

	void prepare(VkRenderPass& render_pass);

	void updateDescriptors(HizPipeline& hiz_pipeline, uint32_t index);

private:
	VkPipelineLayout pipeline_layout{ VK_NULL_HANDLE };

	VkDescriptorSet descriptor_set{ VK_NULL_HANDLE };

	VkPipeline pipeline{ VK_NULL_HANDLE };

	VkDescriptorSetLayout descriptor_set_layout{ VK_NULL_HANDLE };

	VkDescriptorPool descriptor_pool{ VK_NULL_HANDLE };
};