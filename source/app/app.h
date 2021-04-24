#include <vulkanexamplebase.h>

#include <scene/scene_loader.h>
#include <scene/cacher/buffer_cacher.h>
#include <scene/components/transform.h>
#include <scene/components/camera.h>
#include <scene/components/mesh.h>
#include <scene/components/virtual_texture.h>
#include <scene/geometry/frustum.h>

#include <renderer/culling_pipeline.h>
#include <renderer/scene_pipeline.h>
#include <renderer/hiz_pipeline.h>
#include <renderer/debug_pipeline.h>

#include <vk_mem_alloc.h>

//#define HIZ_ENABLE
//#define VIS_HIZ


class Application :public VulkanExampleBase
{
public:
	PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures{};
	VkPhysicalDeviceDepthClipEnableFeaturesEXT physicalDeviceDepthClipEnableFeatures{};
	
public:
	Application();

	~Application();

	void buildCommandBuffers();

	void prepareUniformBuffers();

	void updateUniformBuffers();

	void prepare();

	virtual void getEnabledFeatures() override;

	void draw();

	virtual void render();

	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

	virtual void updateOverlay() override;

	virtual void windowResized() override;

	void saveScreenShot();

private:
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
	SceneUBO last_sceneUBO;

	std::unique_ptr<chaf::Scene> scene{ nullptr };

	std::unique_ptr<CullingPipeline> culling_pipeline;
	std::unique_ptr<ScenePipeline> scene_pipeline;

#ifdef HIZ_ENABLE
	std::unique_ptr<HizPipeline> hiz_pipeline;
#endif // HIZ_ENABLE

#ifdef VIS_HIZ
	std::unique_ptr<DebugPipeline> debug_pipeline;
#endif // VIS_HIZ


	chaf::Frustum frustum;

	int32_t display_debug{ 0 };

#ifdef USE_OCCLUSION_QUERY
	std::vector<uint64_t> pass_sample;
#endif
};