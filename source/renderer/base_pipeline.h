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

		uint32_t getGroupCount(uint32_t thread_count, uint32_t group_size);

		vks::VulkanDevice& device;

		VkPipelineCache pipeline_cache;
		
		std::vector<VkShaderModule> shader_modules;
	};
}