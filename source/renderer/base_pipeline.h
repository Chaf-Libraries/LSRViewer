#pragma once

#include <vulkanexamplebase.h>

namespace chaf
{
	class PipelineBase
	{
	public:
		PipelineBase(vks::VulkanDevice& device);

		virtual ~PipelineBase();

	protected:
		VkPipelineShaderStageCreateInfo PipelineBase::loadShader(std::string fileName, VkShaderStageFlagBits stage);

		vks::VulkanDevice& device;
		
		std::vector<VkShaderModule> shader_modules;


	};
}