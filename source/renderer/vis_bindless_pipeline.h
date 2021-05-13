#pragma once

#include <renderer/base_pipeline.h>

#include <scene/scene.h>

class VisBindlessPipeline :public chaf::PipelineBase
{
public:
	VisBindlessPipeline(vks::VulkanDevice& device, chaf::Scene& scene);

	~VisBindlessPipeline();

	void destroy();

	void prepare(VkRenderPass render_pass, VkQueue queue);

	void setupPipeline(VkRenderPass render_pass);

	void commandRecord(VkCommandBuffer& cmd_buffer);

	void updateDescriptors();
private:
	chaf::Scene& scene;

	uint32_t maxCount = 0;

	VkPipelineLayout pipeline_layout{ VK_NULL_HANDLE };

	VkDescriptorPool descriptor_pool{ VK_NULL_HANDLE };

	VkPipeline pipeline{ VK_NULL_HANDLE };

	VkDescriptorSetLayout descriptor_set_layout{ VK_NULL_HANDLE };

	VkDescriptorSet descriptor_set{ VK_NULL_HANDLE };
};