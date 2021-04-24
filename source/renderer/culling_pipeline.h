#pragma once

#include <renderer/base_pipeline.h>
#include <renderer/hiz_pipeline.h>

#include <scene/scene.h>

#include <glm/glm.hpp>

// TODO: Compute shader Occlusion Culling
#define DEBUG_HIZ

class CullingPipeline: public chaf::PipelineBase
{
public:
	struct InstanceData
	{
		alignas(16) glm::vec3 min{ 0.f };
		alignas(16) glm::vec3 max{ 0.f };
	};

public:
	CullingPipeline(vks::VulkanDevice& device, chaf::Scene& scene);

	~CullingPipeline();

	void buildCommandBuffer();

	void prepare(VkPipelineCache& pipeline_cache, vks::Buffer& uniform_buffer);

	void prepare(VkPipelineCache& pipeline_cache, vks::Buffer& uniform_buffer, HizPipeline& hiz_pipeline);

	void submit();

	void prepareBuffers(VkQueue& queue);

	void updateDescriptorSet(vks::Buffer& uniform_buffer, HizPipeline& hiz_pipeline);

public:
	chaf::Scene& scene;

	vks::Buffer indirect_command_buffer;

	vks::Buffer indircet_draw_count_buffer;

	vks::Buffer instance_buffer;

	vks::Buffer query_result_buffer;

	VkQueue compute_queue{ VK_NULL_HANDLE };

	std::vector<VkDrawIndexedIndirectCommand> indirect_commands;

	VkCommandPool command_pool{ VK_NULL_HANDLE };

	VkCommandBuffer command_buffer{ VK_NULL_HANDLE };

	VkFence fence{ VK_NULL_HANDLE };

	VkSemaphore semaphore{ VK_NULL_HANDLE };

	VkDescriptorSetLayout descriptor_set_layout{ VK_NULL_HANDLE };

	VkDescriptorPool descriptor_pool;

	VkDescriptorSet descriptor_set{ VK_NULL_HANDLE };

	VkPipelineLayout pipeline_layout{ VK_NULL_HANDLE };

	VkPipeline pipeline{ VK_NULL_HANDLE };

	uint32_t primitive_count{ 0 };

#ifdef USE_OCCLUSION_QUERY
	VkQueryPool query_pool;
#endif // USE_OCCLUSION_QUERY

	struct
	{
		std::vector<uint32_t> draw_count;
	}indirect_status;

#ifdef DEBUG_HIZ
	struct
	{
		std::vector<float> depth;
	}debug_depth;

	vks::Buffer debug_depth_buffer;

	struct
	{
		std::vector<float> z;
	}debug_z;

	vks::Buffer debug_z_buffer;
#endif // DEBUG_HIZ


	// Node_ID - <primitive_id, index>
	std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> id_lookup;
	// TODO: LOD
};
