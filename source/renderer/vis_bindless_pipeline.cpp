#include <renderer/vis_bindless_pipeline.h>

VisBindlessPipeline::VisBindlessPipeline(vks::VulkanDevice& device, chaf::Scene& scene) :
	chaf::PipelineBase{ device }, scene{ scene }
{
}

VisBindlessPipeline::~VisBindlessPipeline()
{
	destroy();
}

void VisBindlessPipeline::destroy()
{
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
}

void VisBindlessPipeline::prepare(VkRenderPass render_pass, VkQueue queue)
{
	// Scene Primitive count
	maxCount = scene.images.size() > device.properties.limits.maxPerStageDescriptorUniformBuffers ? device.properties.limits.maxPerStageDescriptorUniformBuffers : static_cast<uint32_t>(scene.images.size());

	// Prepare descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxCount)
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptor_pool));

	// Prepare descriptor set layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// binding all textures
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 0, maxCount)
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
	descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
	descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{};
	setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	setLayoutBindingFlags.bindingCount = 1;
	std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags = {
		0,
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
	};
	setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();
	descriptorSetLayoutCI.pNext = &setLayoutBindingFlags;

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptor_set_layout));

	std::array<VkDescriptorSetLayout, 1> setLayouts = { descriptor_set_layout };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	std::vector<VkPushConstantRange> pushConstantRanges = {
		vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(int32_t), 0)
	};

	pipelineLayoutCI.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
	pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipeline_layout));

	// Descriptor set for bindless object
	VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo = {};

	uint32_t variableDescCounts[] = { maxCount };

	variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
	variableDescriptorCountAllocInfo.descriptorSetCount = 0;
	variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &descriptor_set_layout, 1);
	allocInfo.pNext = &variableDescriptorCountAllocInfo;

	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set));

	updateDescriptors();

	setupPipeline(render_pass);
}

void VisBindlessPipeline::setupPipeline(VkRenderPass render_pass)
{
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
	}
	// Pipeline state setup
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		vks::initializers::vertexInputBindingDescription(0, sizeof(chaf::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
		vks::initializers::vertexInputBindingDescription(1, sizeof(uint32_t), VK_VERTEX_INPUT_RATE_INSTANCE),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, pos)),
		vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, normal)),
		vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, uv)),
		vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, color)),
		vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, tangent)),
		// Instance Index
		vks::initializers::vertexInputAttributeDescription(1, 5, VK_FORMAT_R32_UINT, sizeof(uint32_t)),
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipeline_layout, render_pass, 0);
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
	shaderStages.resize(2);
	shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/vis_bindless.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/vis_bindless.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipeline_cache, 1, &pipelineCI, nullptr, &pipeline));
}

void VisBindlessPipeline::commandRecord(VkCommandBuffer& cmd_buffer)
{
	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	vkCmdPushConstants(cmd_buffer, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int32_t), &maxCount);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

	vkCmdDraw(cmd_buffer, 6, 1, 0, 0);
}

void VisBindlessPipeline::updateDescriptors()
{
	std::vector<VkDescriptorImageInfo> textureDescriptors(maxCount);
	for (size_t i = 0; i < maxCount; i++)
	{
		textureDescriptors[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// TODO: Use Image Cacher
		textureDescriptors[i].sampler = scene.images[i].texture.sampler;
		textureDescriptors[i].imageView = scene.images[i].texture.view;
	}

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.dstArrayElement = 0;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet.descriptorCount = maxCount;
	writeDescriptorSet.dstSet = descriptor_set;
	writeDescriptorSet.pImageInfo = textureDescriptors.data();

	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}
