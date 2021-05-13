#include <renderer/scene_pipeline.h>

#include <scene/node.h>
#include <scene/components/mesh.h>
#include <scene/components/transform.h>

ScenePipeline::ScenePipeline(vks::VulkanDevice& device, chaf::Scene& scene) :
	chaf::PipelineBase{ device }, scene{ scene }
{
}

ScenePipeline::~ScenePipeline()
{
	sceneUBO.buffer.destroy();
	last_sceneUBO_buffer.destroy();
	instanceIndexBuffer.destroy();

	destroy();
}

void ScenePipeline::destroy()
{
	if (has_init)
	{
		vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
		vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.scene, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptor_set_layouts.object, nullptr);
		has_init = false;
	}
}

void ScenePipeline::prepare(VkRenderPass render_pass, VkQueue queue)
{
	// Scene Primitive count
	maxCount = scene.images.size() > device.properties.limits.maxPerStageDescriptorUniformBuffers ? device.properties.limits.maxPerStageDescriptorUniformBuffers : static_cast<uint32_t>(scene.images.size());

	vks::Buffer stagingBuffer;

	std::vector<uint32_t> instanceData(maxCount);
	for (uint32_t i = 0; i < maxCount; i++)
	{
		instanceData[i] = i;
	}

	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		instanceData.size() * sizeof(uint32_t),
		instanceData.data()));

	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&instanceIndexBuffer,
		stagingBuffer.size));

	device.copyBuffer(&stagingBuffer, &instanceIndexBuffer, queue);

	stagingBuffer.destroy();

	// Prepare descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxCount)
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxCount + 2);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptor_pool));

	// Prepare descriptor set layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding Scene UBO
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 0),
		// binding all model matrices
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 1),
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptor_set_layouts.scene));

	setLayoutBindings = {
		// binding all textures
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 0, maxCount)
	};

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

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptor_set_layouts.object));

	std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptor_set_layouts.scene, descriptor_set_layouts.object };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipeline_layout));

	// Create scene UBO
	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&sceneUBO.buffer,
		sizeof(sceneUBO.values)));
	VK_CHECK_RESULT(sceneUBO.buffer.map());

	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&last_sceneUBO_buffer,
		sizeof(sceneUBO.values)));
	VK_CHECK_RESULT(last_sceneUBO_buffer.map());
	 
	// Descriptor set for scene UBO
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &descriptor_set_layouts.scene, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set.scene));

	std::vector<VkWriteDescriptorSet>writeDescriptorSets = {
		vks::initializers::writeDescriptorSet(descriptor_set.scene, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &sceneUBO.buffer.descriptor),
		vks::initializers::writeDescriptorSet(descriptor_set.scene, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &scene.object_buffer.descriptor)
	};

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

	// Descriptor set for bindless object
	VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo = {};

	uint32_t variableDescCounts[] = { maxCount };

	variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
	variableDescriptorCountAllocInfo.descriptorSetCount = 1;
	variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

	allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &descriptor_set_layouts.object, 1);
	allocInfo.pNext = &variableDescriptorCountAllocInfo;

	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set.object));

	updateDescriptors();

	setupPipeline(render_pass);	

	has_init = true;
}

void ScenePipeline::setupPipeline(VkRenderPass render_pass)
{
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
	}
	// Pipeline state setup
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI;
	if (enable_tessellation)
	{
		inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 0, VK_FALSE);
	}
	else
	{
		inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	}

	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(line_mode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
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
	if (enable_tessellation)
	{
		VkPipelineTessellationStateCreateInfo tessellationStateCI = vks::initializers::pipelineTessellationStateCreateInfo(3);
		pipelineCI.pTessellationState = &tessellationStateCI;

		shaderStages.resize(4);
		shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/scene_indexing_tes.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/scene_indexing_tes.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderStages[2] = loadShader("../data/shaders/glsl/gpudrivenpipeline/scene_indexing_tes.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		shaderStages[3] = loadShader("../data/shaders/glsl/gpudrivenpipeline/scene_indexing_tes.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	}
	else
	{
		shaderStages.resize(2);
		shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/scene_indexing.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/scene_indexing.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipeline_cache, 1, &pipelineCI, nullptr, &pipeline));
}

void ScenePipeline::commandRecord(VkCommandBuffer& cmd_buffer, CullingPipeline& culling_pipeline)
{
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set.scene, 0, nullptr);
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 1, 1, &descriptor_set.object, 0, nullptr);
	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkDeviceSize offsets[1] = { 0 };

	if (scene.buffer_cacher->hasVBO(0) && scene.buffer_cacher->hasEBO(0))
	{
		vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &scene.buffer_cacher->getVBO(0).buffer, offsets);
		vkCmdBindVertexBuffers(cmd_buffer, 1, 1, &instanceIndexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(cmd_buffer, scene.buffer_cacher->getEBO(0).buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	if (scene.buffer_cacher->hasVBO(0) && scene.buffer_cacher->hasEBO(0))
	{
		if (device.features.multiDrawIndirect)
		{
			vkCmdDrawIndexedIndirect(cmd_buffer, culling_pipeline.indirect_command_buffer.buffer, 0, static_cast<uint32_t>(culling_pipeline.indirect_commands.size()), sizeof(VkDrawIndexedIndirectCommand));
		}
		else
		{
			// If multi draw is not available, we must issue separate draw commands
			for (auto j = 0; j < culling_pipeline.indirect_commands.size(); j++)
			{
				vkCmdDrawIndexedIndirect(cmd_buffer, culling_pipeline.indirect_command_buffer.buffer, j * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
			}
		}
	}
}

void ScenePipeline::updateDescriptors()
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
	writeDescriptorSet.dstSet = descriptor_set.object;
	writeDescriptorSet.pImageInfo = textureDescriptors.data();

	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}

void ScenePipeline::setupDepth(uint32_t width, uint32_t height, VkFormat depthFormat, VkQueue queue)
{
	depth_image.width = width;
	depth_image.height = height;

	// Setup depth image
	VkImageCreateInfo image = vks::initializers::imageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.extent.width = width;
	image.extent.height = height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.format = depthFormat;																// Depth stencil attachment
	image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;		// We will sample directly from the depth attachment for the shadow mapping
	VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &depth_image.image));

	VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	VkCommandBuffer layoutCmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Setup memory
	vkGetImageMemoryRequirements(device, depth_image.image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &depth_image.mem));
	VK_CHECK_RESULT(vkBindImageMemory(device, depth_image.image, depth_image.mem, 0));

	// Setup image barrier
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;
	vks::tools::setImageLayout(
		layoutCmd,
		depth_image.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange);

	device.flushCommandBuffer(layoutCmd, queue, true);

	// Setup view
	VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = depthFormat;
	view.components = { VK_COMPONENT_SWIZZLE_R };
	view.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT , 0, 1, 0, 1 };
	view.subresourceRange.layerCount = 1;
	view.image = depth_image.image;
	VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &depth_image.view));

	// Setup sampler
	VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(device.logicalDevice, &sampler, nullptr, &depth_image.sampler));

	// Setup descriptor
	depth_image.descriptor =
		vks::initializers::descriptorImageInfo(
			depth_image.sampler,
			depth_image.view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ScenePipeline::copyDepth(VkCommandBuffer cmd_buffer, VkImage depth_stencil_image)
{
	vks::tools::setImageLayout(
		cmd_buffer,
		depth_stencil_image,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	// Change image layout of one cubemap face to transfer destination
	vks::tools::setImageLayout(
		cmd_buffer,
		depth_image.image,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy region for transfer from framebuffer to cube face
	VkImageCopy copyRegion = {};

	copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	copyRegion.srcSubresource.baseArrayLayer = 0;
	copyRegion.srcSubresource.mipLevel = 0;
	copyRegion.srcSubresource.layerCount = 1;
	copyRegion.srcOffset = { 0, 0, 0 };

	copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	copyRegion.dstSubresource.baseArrayLayer = 0;
	copyRegion.dstSubresource.mipLevel = 0;
	copyRegion.dstSubresource.layerCount = 1;
	copyRegion.dstOffset = { 0, 0, 0 };

	copyRegion.extent.width = depth_image.width;
	copyRegion.extent.height = depth_image.height;
	copyRegion.extent.depth = 1;

	// Put image copy into command buffer
	vkCmdCopyImage(
		cmd_buffer,
		depth_stencil_image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		depth_image.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&copyRegion);

	// Transform framebuffer color attachment back
	vks::tools::setImageLayout(
		cmd_buffer,
		depth_stencil_image,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	// Change image layout of copied face to shader read
	vks::tools::setImageLayout(
		cmd_buffer,
		depth_image.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange);
}

void ScenePipeline::resize(uint32_t width, uint32_t height, VkFormat depthFormat, VkQueue queue)
{
	vkDestroySampler(device, depth_image.sampler, nullptr);
	vkDestroyImageView(device, depth_image.view, nullptr);
	vkDestroyImage(device, depth_image.image, nullptr);
	vkFreeMemory(device, depth_image.mem, nullptr);

	setupDepth(width, height, depthFormat, queue);
}