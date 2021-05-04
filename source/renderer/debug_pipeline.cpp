#include <renderer/debug_pipeline.h>

DebugPipeline::DebugPipeline(vks::VulkanDevice& device) :
	PipelineBase{ device }
{
}

DebugPipeline::~DebugPipeline()
{
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
}

void DebugPipeline::buildCommandBuffer(VkCommandBuffer& cmd_buffer, VkRenderPassBeginInfo& renderPassBeginInfo)
{
	//vkCmdBeginRenderPass(cmd_buffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

	vkCmdDraw(cmd_buffer, 6, 1, 0, 0);

	//vkCmdEndRenderPass(cmd_buffer);
}

void DebugPipeline::setupDescriptors(HizPipeline& hiz_pipeline)
{
	// Prepare descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};

	// Setting descriptor pool
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device.logicalDevice, &descriptorPoolInfo, nullptr, &descriptor_pool));

	// Setting layout bindings
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: render image
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0)
	};

	// Create descriptor layout
	VkDescriptorSetLayoutCreateInfo descriptorLayout =
		vks::initializers::descriptorSetLayoutCreateInfo(
			setLayoutBindings.data(),
			static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorLayout, nullptr, &descriptor_set_layout));

	// Pipeline layout
	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vks::initializers::pipelineLayoutCreateInfo(
			&descriptor_set_layout,
			1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device.logicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipeline_layout));

	// Descriptor sets
	VkDescriptorSetAllocateInfo allocInfo =
		vks::initializers::descriptorSetAllocateInfo(
			descriptor_pool,
			&descriptor_set_layout,
			1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set));

	VkDescriptorImageInfo dstTarget;
	dstTarget.sampler = hiz_pipeline.hiz_image.sampler;
	dstTarget.imageView = hiz_pipeline.hiz_image.views[0];
	dstTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Update descriptor set
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &dstTarget),
	};

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

//void DebugPipeline::setupDescriptors(HizPipeline& hiz_pipeline)
//{
//	// Prepare descriptor pool
//	std::vector<VkDescriptorPoolSize> poolSizes = {
//		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
//	};
//
//	// Setting descriptor pool
//	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
//	VK_CHECK_RESULT(vkCreateDescriptorPool(device.logicalDevice, &descriptorPoolInfo, nullptr, &descriptor_pool));
//
//	// Setting layout bindings
//	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
//		// Binding 0: render image
//		vks::initializers::descriptorSetLayoutBinding(
//			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//			VK_SHADER_STAGE_FRAGMENT_BIT,
//			0)
//	};
//
//	// Create descriptor layout
//	VkDescriptorSetLayoutCreateInfo descriptorLayout =
//		vks::initializers::descriptorSetLayoutCreateInfo(
//			setLayoutBindings.data(),
//			static_cast<uint32_t>(setLayoutBindings.size()));
//
//	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorLayout, nullptr, &descriptor_set_layout));
//
//	// Pipeline layout
//	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
//		vks::initializers::pipelineLayoutCreateInfo(
//			&descriptor_set_layout,
//			1);
//	VK_CHECK_RESULT(vkCreatePipelineLayout(device.logicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipeline_layout));
//
//	// Descriptor sets
//	VkDescriptorSetAllocateInfo allocInfo =
//		vks::initializers::descriptorSetAllocateInfo(
//			descriptor_pool,
//			&descriptor_set_layout,
//			1);
//
//	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set));
//
//	VkDescriptorImageInfo dstTarget;
//	dstTarget = hiz_pipeline.depth_image.descriptor;
//
//	// Update descriptor set
//	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
//			vks::initializers::writeDescriptorSet(descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &dstTarget),
//	};
//
//	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
//}

void DebugPipeline::prepare(VkPipelineCache& pipeline_cache, VkRenderPass& render_pass)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo({}, {});

	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipeline_layout, render_pass, 0);
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/debug.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/debug.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipeline_cache, 1, &pipelineCI, nullptr, &pipeline));
}

void DebugPipeline::updateDescriptors(HizPipeline& hiz_pipeline, uint32_t index)
{
	VkDescriptorImageInfo dstTarget;
	dstTarget.sampler = hiz_pipeline.hiz_image.sampler;
	dstTarget.imageView = hiz_pipeline.hiz_image.views[index];
	dstTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Update descriptor set
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &dstTarget),
	};

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
