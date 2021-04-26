#pragma once

#include <renderer/base_pipeline.h>
#include <renderer/culling_pipeline.h>
#include <renderer/hiz_pipeline.h>

#include <scene/geometry/frustum.h>

#include <scene/scene.h>

// Feature switch
//#define ENABLE_DEFERRED
#define ENABLE_DESCRIPTOR_INDEXING
#define USE_TESSELLATION

class ScenePipeline :public chaf::PipelineBase
{
public:
	ScenePipeline(vks::VulkanDevice& device, chaf::Scene& scene);

	~ScenePipeline();

	void bindCommandBuffers(VkCommandBuffer& cmd_buffer, chaf::Frustum& frustum);

	void bindCommandBuffers(VkCommandBuffer& cmd_buffer, CullingPipeline& culling_pipeline);

	void setupDescriptors(vks::Buffer& uniform_buffer);

	void setupDescriptors(VkDescriptorPool& descriptorPool, vks::Buffer& uniform_buffer, HizPipeline& hiz_pipeline);

	void preparePipelines(VkPipelineCache& pipeline_cache, VkRenderPass& render_pass);

	void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, CullingPipeline& culling_pipeline);

	void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, chaf::Frustum& frustum);

public:
	chaf::Scene& scene;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

	VkDescriptorPool descriptor_pool{ VK_NULL_HANDLE };

	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

	VkPipeline pipeline{ VK_NULL_HANDLE };

	std::vector<VkShaderModule> shader_modules;

	struct 
	{
		VkDescriptorSetLayout matrices{ VK_NULL_HANDLE };
		VkDescriptorSetLayout textures{ VK_NULL_HANDLE };
	} descriptorSetLayouts;

	struct
	{
		VkSampler sampler{ VK_NULL_HANDLE };
		VkImageView view{ VK_NULL_HANDLE };
		VkImage image{ VK_NULL_HANDLE };
		VkDeviceMemory mem{ VK_NULL_HANDLE };
		VkDescriptorImageInfo descriptor;
		uint32_t width, height;
	} depth_image;

	void setupDepth(uint32_t width, uint32_t height, VkFormat depthFormat, VkQueue queue);

	void copyDepth(VkCommandBuffer cmd_buffer, VkImage depth_stencil_image);

	void resize(uint32_t width, uint32_t height, VkFormat depthFormat, VkQueue queue);

	// TODO: beta - deferred rendering
#ifdef ENABLE_DEFERRED
public:
	struct FrameBufferAttachment
	{
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
		VkFormat format;
		uint32_t width, height;
	};

	struct
	{
		VkPipeline pipeline;
		VkRenderPass render_pass;
		FrameBufferAttachment readback;
		FrameBufferAttachment depth;
		VkFramebuffer frame_buffer;
		VkSampler sampler;
		VkCommandBuffer cmd_buffer;
		VkSemaphore semaphore;
	}deferred_pipeline;

public:
	void createAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment* attachment, uint32_t width, uint32_t height);
	void prepareDeferredFramebuffer();
	void buildDeferredCommandBuffer();
#endif // ENABLE_DEFERRED
};