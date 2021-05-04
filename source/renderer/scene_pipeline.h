#pragma once

#include <renderer/base_pipeline.h>
#include <renderer/culling_pipeline.h>
#include <renderer/hiz_pipeline.h>

#include <scene/geometry/frustum.h>

#include <scene/scene.h>

class ScenePipeline :public chaf::PipelineBase
{
public:
	ScenePipeline(vks::VulkanDevice& device, chaf::Scene& scene);

	~ScenePipeline();

	void destroy();

	void prepare(VkRenderPass render_pass, VkQueue queue);

	void setupPipeline(VkRenderPass render_pass);

	void CommandRecord(VkCommandBuffer& cmd_buffer, CullingPipeline& culling_pipeline);

public:
	struct SceneUBO
	{
		vks::Buffer buffer;
		struct
		{
			glm::mat4 projection;
			glm::mat4 view;
			glm::vec4 lightPos = glm::vec4(0.0f, 2.5f, 0.0f, 1.0f);
			glm::vec4 viewPos;
			glm::vec4 frustum[6];
			glm::vec4 range; // (width, height, far, near)
		}values;
	};

	SceneUBO sceneUBO;
	vks::Buffer last_sceneUBO_buffer;

public:
	chaf::Scene& scene;

	VkPipelineLayout pipeline_layout{ VK_NULL_HANDLE };

	VkDescriptorPool descriptor_pool{ VK_NULL_HANDLE };

	VkPipeline pipeline{ VK_NULL_HANDLE };

	vks::Buffer instanceIndexBuffer;

	struct
	{
		VkDescriptorSetLayout scene{ VK_NULL_HANDLE };
		VkDescriptorSetLayout object{ VK_NULL_HANDLE };
	}descriptor_set_layouts;

	struct
	{
		VkDescriptorSet scene{ VK_NULL_HANDLE };
		VkDescriptorSet object{ VK_NULL_HANDLE };
	}descriptor_set;

	bool has_init{ false };

	bool enable_tessellation{ false };

	bool line_mode{ false };

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
};