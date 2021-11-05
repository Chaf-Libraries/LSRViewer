#include <renderer/hiz_pipeline.h>

HizPipeline::HizPipeline(vks::VulkanDevice& device, uint32_t width, uint32_t height) :
	PipelineBase{ device }
{
	depth_image.width = width;
	depth_image.height = height;

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = device.queueFamilyIndices.compute;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &command_pool));

	vkGetDeviceQueue(device, device.queueFamilyIndices.compute, 0, &compute_queue);
}

HizPipeline::~HizPipeline()
{
	// Destroy depth image
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

	destroyDepth();
	destroyHiz();

	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyCommandPool(device, command_pool, nullptr);

	vkDestroyFence(device, fence, nullptr);
	vkDestroySemaphore(device, semaphore, nullptr);
}

void HizPipeline::prepareHiz()
{
	hiz_image.depth_pyramid_levels = static_cast<uint32_t>(floor(log2(std::max(depth_image.width, depth_image.height))));

	// Setup Hi-z image
	VkImageCreateInfo image = vks::initializers::imageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.extent.width = depth_image.width;
	image.extent.height = depth_image.height;
	image.extent.depth = 1;
	image.mipLevels = hiz_image.depth_pyramid_levels;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.format = VK_FORMAT_R32_SFLOAT;																// Depth stencil attachment
	image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;		// We will sample directly from the depth attachment for the shadow mapping
	image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &hiz_image.image));

	VkCommandBuffer layoutCmd = device.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, command_pool, true);

	// Setup Hi-z image memory
	VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, hiz_image.image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &hiz_image.mem));
	VK_CHECK_RESULT(vkBindImageMemory(device, hiz_image.image, hiz_image.mem, 0));

	for (uint32_t i = 0; i < hiz_image.depth_pyramid_levels; i++)
	{
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = i;
		subresourceRange.levelCount = hiz_image.depth_pyramid_levels - i;
		subresourceRange.layerCount = 1;
		vks::tools::setImageLayout(
			layoutCmd,
			hiz_image.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);
	}

	device.flushCommandBuffer(layoutCmd, compute_queue, command_pool, true);

	// Setup view
	VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = VK_FORMAT_R32_SFLOAT;
	view.components = { VK_COMPONENT_SWIZZLE_R };
	view.subresourceRange.layerCount = 1;
	view.image = hiz_image.image;

	hiz_image.views.resize(hiz_image.depth_pyramid_levels);
	for (uint32_t i = 0; i < hiz_image.views.size(); i++)
	{
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT , i, hiz_image.depth_pyramid_levels - i, 0, 1 };
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &hiz_image.views[i]));
	}
	
	VkSamplerCreateInfo sampler = {};

	//fill the normal stuff
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.minLod = 0; 
	//sampler.maxLod = static_cast<float>(hiz_image.depth_pyramid_levels);
	sampler.maxLod = FLT_MAX;

#ifdef ENABLE_DYNAMIC_STATE
	VkSamplerReductionModeCreateInfoEXT createInfoReduction = {};

	createInfoReduction.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT;
	createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;
	sampler.pNext = &createInfoReduction;
#endif // ENABLE_DYNAMIC_STATE

	VK_CHECK_RESULT(vkCreateSampler(device, &sampler, 0, &hiz_image.sampler));
}

void HizPipeline::destroyHiz()
{
	for (auto& view : hiz_image.views)
	{
		vkDestroyImageView(device, view, nullptr);
	}

	vkDestroyImage(device, hiz_image.image, nullptr);
	vkFreeMemory(device, hiz_image.mem, nullptr);
	vkDestroySampler(device, hiz_image.sampler, nullptr);
}

void HizPipeline::prepareDepth(VkQueue queue, VkFormat depth_format)
{
	// Setup depth image
	depth_image.format = depth_format;

	VkImageCreateInfo image = vks::initializers::imageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.extent.width = depth_image.width;
	image.extent.height = depth_image.height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.format = depth_format;																// Depth stencil attachment
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
	view.format = depth_format;
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

void HizPipeline::destroyDepth()
{
	vkDestroySampler(device, depth_image.sampler, nullptr);
	vkDestroyImageView(device, depth_image.view, nullptr);
	vkDestroyImage(device, depth_image.image, nullptr);
	vkFreeMemory(device, depth_image.mem, nullptr);
}

void HizPipeline::copyDepth(VkCommandBuffer cmd_buffer, VkImage depth_stencil_image)
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

void HizPipeline::resize(uint32_t width, uint32_t height, VkQueue queue)
{
	depth_image.width = width;
	depth_image.height = height;

	destroyDepth();
	prepareDepth(queue,depth_image.format);

	destroyHiz();
	prepareHiz();

	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

	// Prepare descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, hiz_image.depth_pyramid_levels),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, hiz_image.depth_pyramid_levels)
	};

	// Setting descriptor pool
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, hiz_image.depth_pyramid_levels);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device.logicalDevice, &descriptorPoolInfo, nullptr, &descriptor_pool));

	// Descriptor sets
	descriptor_sets.resize(hiz_image.depth_pyramid_levels);
	VkDescriptorSetAllocateInfo allocInfo =
		vks::initializers::descriptorSetAllocateInfo(
			descriptor_pool,
			&descriptor_set_layout,
			1);

	for (uint32_t i = 0; i < descriptor_sets.size(); i++)
	{
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_sets[i]));
	}

	buildCommandBuffer();
}

void HizPipeline::prepare(VkQueue queue, VkFormat depth_format)
{
	prepareDepth(queue, depth_format);
	prepareHiz();

	// Prepare descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, hiz_image.depth_pyramid_levels),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, hiz_image.depth_pyramid_levels)
	};

	// Setting descriptor pool
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, hiz_image.depth_pyramid_levels);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device.logicalDevice, &descriptorPoolInfo, nullptr, &descriptor_pool));

	// Setting layout bindings
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Store output image
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_SHADER_STAGE_COMPUTE_BIT,
			0),
		// Binding 1: Sample input image
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_COMPUTE_BIT,
			1)
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

	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(glm::vec2), 0);
	// Push constant ranges are part of the pipeline layout
	pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	VK_CHECK_RESULT(vkCreatePipelineLayout(device.logicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipeline_layout));

	// Descriptor sets
	descriptor_sets.resize(hiz_image.depth_pyramid_levels);
	VkDescriptorSetAllocateInfo allocInfo =
		vks::initializers::descriptorSetAllocateInfo(
			descriptor_pool,
			&descriptor_set_layout,
			1);

	for (uint32_t i = 0; i < descriptor_sets.size(); i++)
	{
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_sets[i]));
	}

	// Create pipeline
	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipeline_layout, 0);
	computePipelineCreateInfo.stage = loadShader("../data/shaders/glsl/gpudrivenpipeline/hiz.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);;

	VK_CHECK_RESULT(vkCreateComputePipelines(device.logicalDevice, pipeline_cache, 1, &computePipelineCreateInfo, nullptr, &pipeline));

	// Create a command buffer for compute operations
	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vks::initializers::commandBufferAllocateInfo(
			command_pool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device.logicalDevice, &cmdBufAllocateInfo, &command_buffer));

	// Fence for compute CB sync
	VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK_RESULT(vkCreateFence(device.logicalDevice, &fenceCreateInfo, nullptr, &fence));

	VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
	VK_CHECK_RESULT(vkCreateSemaphore(device.logicalDevice, &semaphoreCreateInfo, nullptr, &semaphore));

	buildCommandBuffer();
}

void HizPipeline::buildCommandBuffer()
{
	// Begin recording command buffer
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(command_buffer, &cmdBufInfo));

	// Bind pipeline
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	// Bind descriptor & dispatch task

	for (uint32_t i = 0; i < hiz_image.depth_pyramid_levels; i++)
	{
		VkDescriptorImageInfo dstTarget;
		dstTarget.sampler = depth_image.sampler;
		dstTarget.imageView = hiz_image.views[i];
		dstTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkDescriptorImageInfo srcTarget;
		srcTarget.sampler = depth_image.sampler;

		if (i == 0)
		{
			srcTarget.imageView = depth_image.view;
			srcTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		else
		{
			srcTarget.imageView = hiz_image.views[i - 1];
			srcTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}

		// Update descriptor set
		std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets =
		{
			// Binding 0: Store output image
			vks::initializers::writeDescriptorSet(
				descriptor_sets[i],
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				0,
				&dstTarget),
			// Binding 1: Sample input image
			vks::initializers::writeDescriptorSet(
				descriptor_sets[i],
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&srcTarget)
		};

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, NULL);

		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_sets[i], 0, nullptr);

		uint32_t levelWidth = depth_image.width >> i;
		uint32_t levelHeight = depth_image.height >> i;

		if (levelHeight < 1) levelHeight = 1;
		if (levelWidth < 1) levelWidth = 1;

		glm::vec2 reduce_data = { levelWidth, levelHeight };

		vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::vec2), &reduce_data);
		vkCmdDispatch(command_buffer, getGroupCount(levelWidth, 16), getGroupCount(levelHeight, 16), 1);

		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageMemoryBarrier.image = hiz_image.image;
		imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT , 0, 1, 0, 1};
		imageMemoryBarrier.srcQueueFamilyIndex = device.queueFamilyIndices.compute;
		imageMemoryBarrier.dstQueueFamilyIndex = device.queueFamilyIndices.compute;
		vkCmdPipelineBarrier(
			command_buffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_DEPENDENCY_BY_REGION_BIT,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
	}

	vkEndCommandBuffer(command_buffer);

	hiz_image.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	hiz_image.descriptor.imageView = hiz_image.views[0];
	hiz_image.descriptor.sampler = hiz_image.sampler;
}

void HizPipeline::submit()
{
	vkWaitForFences(device.logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device.logicalDevice, 1, &fence);

	VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &command_buffer;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &semaphore;

	VK_CHECK_RESULT(vkQueueSubmit(compute_queue, 1, &computeSubmitInfo, VK_NULL_HANDLE));
}
