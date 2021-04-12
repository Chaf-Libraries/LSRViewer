#include <renderer/base_pipeline.h>

namespace chaf
{
	PipelineBase::PipelineBase(vks::VulkanDevice& device) :
		device{ device }
	{
	}

	PipelineBase::~PipelineBase()
	{
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
}