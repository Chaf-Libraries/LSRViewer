#include <vulkanexamplebase.h>

#include <scene/scene_loader.h>
#include <scene/cacher/buffer_cacher.h>
#include <scene/components/transform.h>
#include <scene/components/camera.h>
#include <scene/components/mesh.h>
#include <scene/geometry/frustum.h>

#include <renderer/culling_pipeline.h>
#include <renderer/scene_pipeline.h>

class Application :public VulkanExampleBase
{
public:
	Application();

	~Application();
	void buildCommandBuffers();

	void setupDescriptors();

	void prepareUniformBuffers();

	void updateUniformBuffers();

	void prepare();

	virtual void getEnabledFeatures();

	void draw();

	virtual void render();

	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay);

	virtual void updateOverlay();
private:
	struct
	{
		vks::Buffer buffer;
		struct
		{
			glm::mat4 projection;
			glm::mat4 view;
			glm::vec4 lightPos = glm::vec4(0.0f, 2.5f, 0.0f, 1.0f);
			glm::vec4 viewPos;
			glm::vec4 frustum[6];
		}values;
	}sceneUBO;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;

	struct DescriptorSetLayouts {
		VkDescriptorSetLayout matrices;
		VkDescriptorSetLayout textures;
	} descriptorSetLayouts;

	std::unique_ptr<chaf::Scene> scene{ nullptr };

	std::unique_ptr<CullingPipeline> culling_pipeline;
	std::unique_ptr<ScenePipeline> scene_pipeline;

	chaf::Frustum frustum;

	uint32_t total_cull_cpu{ 0 };
	uint32_t total_cull_gpu{ 0 };
};