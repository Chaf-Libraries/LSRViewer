#pragma once

#include <renderer/base_pipeline.h>

#include <scene/scene.h>

#include <glm/glm.hpp>

// TODO: Compute shader Occlusion Culling

class CullingPipeline: public chaf::PipelineBase
{
public:
	struct InstanceData
	{
		glm::vec3 min{ 0.f };
		glm::vec3 max{ 0.f };
	};
public:
	CullingPipeline(vks::VulkanDevice& device, chaf::Scene& scene);

	~CullingPipeline();

	void buildCommandBuffer();

	void prepare(VkPipelineCache& pipeline_cache, VkDescriptorPool& descriptor_pool, vks::Buffer& uniform_buffer);

	void submit();

	void prepareBuffers(VkQueue& queue);

public:
	chaf::Scene& scene;
	vks::Buffer indirect_command_buffer;
	vks::Buffer indircet_draw_count_buffer;
	vks::Buffer instance_buffer;
	VkQueue compute_queue{ VK_NULL_HANDLE };
	std::vector<VkDrawIndexedIndirectCommand> indirect_commands;
	VkCommandPool command_pool{ VK_NULL_HANDLE };
	VkCommandBuffer command_buffer{ VK_NULL_HANDLE };
	VkFence fence{ VK_NULL_HANDLE };
	VkSemaphore semaphore{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptor_set_layout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptor_set{ VK_NULL_HANDLE };
	VkPipelineLayout pipeline_layout{ VK_NULL_HANDLE };
	VkPipeline pipeline{ VK_NULL_HANDLE };
	uint32_t object_count{ 0 };

	struct
	{
		std::vector<uint32_t> draw_count;
	}indirect_status;

	// ID - index
	std::unordered_map<uint32_t, uint32_t> id_lookup;
	// TODO: LOD
};