#pragma once

#include <renderer/base_pipeline.h>
#include <renderer/culling_pipeline.h>

#include <scene/geometry/frustum.h>

#include <scene/scene.h>

class ScenePipeline :public chaf::PipelineBase
{
public:
	// Extension
	PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
	PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;

public:
	ScenePipeline(vks::VulkanDevice& device, chaf::Scene& scene);

	~ScenePipeline();

	void bindCommandBuffers(VkCommandBuffer& cmd_buffer, chaf::Frustum& frustum);

	void bindCommandBuffers(VkCommandBuffer& cmd_buffer, CullingPipeline& culling_pipeline);

	void setupDescriptors(VkDescriptorPool& descriptorPool, vks::Buffer& uniform_buffer);

	void preparePipelines(VkPipelineCache& pipeline_cache, VkRenderPass& render_pass);

	void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, CullingPipeline& culling_pipeline);

	void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, chaf::Frustum& frustum);
public:
	chaf::Scene& scene;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

	VkPipeline pipeline{ VK_NULL_HANDLE };

	std::vector<VkShaderModule> shader_modules;

	struct 
	{
		VkDescriptorSetLayout matrices{ VK_NULL_HANDLE };
		VkDescriptorSetLayout textures{ VK_NULL_HANDLE };
	} descriptorSetLayouts;
};