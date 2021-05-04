#include <renderer/base_pipeline.h>

namespace chaf
{
	PipelineBase::PipelineBase(vks::VulkanDevice& device) :
		device{ device }
	{
		VkPipelineCacheCreateInfo pipelineCacheCI{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
		vkCreatePipelineCache(device, &pipelineCacheCI, nullptr, &pipeline_cache);
	}

	PipelineBase::~PipelineBase()
	{
		vkDestroyPipelineCache(device, pipeline_cache, nullptr);

		for (auto& shader_module : shader_modules)
		{
			vkDestroyShaderModule(device.logicalDevice, shader_module, nullptr);
		}
		shader_modules.clear();
	}

	VkPipelineShaderStageCreateInfo PipelineBase::loadShader(std::string fileName, VkShaderStageFlagBits stage)
	{
		VkPipelineShaderStageCreateInfo shaderStage = {};
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = stage;
		shaderStage.module = vks::tools::loadShader(fileName.c_str(), device);
		shaderStage.pName = "main";
		assert(shaderStage.module != VK_NULL_HANDLE);
		shader_modules.push_back(shaderStage.module);
		return shaderStage;
	}

	uint32_t PipelineBase::getGroupCount(uint32_t thread_count, uint32_t group_size)
	{
		return (thread_count + group_size - 1) / group_size;
	}
}