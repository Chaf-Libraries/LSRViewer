#include <renderer/culling_pipeline.h>

#include <scene/node.h>
#include <scene/components/mesh.h>
#include <scene/components/transform.h>

CullingPipeline::CullingPipeline(vks::VulkanDevice& device, chaf::Scene& scene) :
	PipelineBase{ device },
	scene{scene}
{
}

CullingPipeline::~CullingPipeline()
{
	indirect_command_buffer.destroy();
	instance_buffer.destroy();
	indircet_draw_count_buffer.destroy();

	vkDestroyPipelineLayout(device.logicalDevice, pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(device.logicalDevice, descriptor_set_layout, nullptr);
	vkDestroyPipeline(device.logicalDevice, pipeline, nullptr);
	vkDestroyFence(device.logicalDevice, fence, nullptr);
	vkDestroyCommandPool(device.logicalDevice, command_pool, nullptr);
	vkDestroySemaphore(device.logicalDevice, semaphore, nullptr);
}

void CullingPipeline::buildCommandBuffer()
{
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VK_CHECK_RESULT(vkBeginCommandBuffer(command_buffer, &cmdBufInfo));

	// TODO: need a barrier?
	//if (device.queueFamilyIndices.graphics != device.queueFamilyIndices.compute)
	//{
	//	VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
	//	bufferBarrier.buffer = indirect_command_buffer.buffer;
	//	bufferBarrier.size = indirect_command_buffer.descriptor.range;
	//	bufferBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	//	bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	//	bufferBarrier.srcQueueFamilyIndex = device.queueFamilyIndices.graphics;
	//	bufferBarrier.dstQueueFamilyIndex = device.queueFamilyIndices.compute;

	//	vkCmdPipelineBarrier(
	//		command_buffer,
	//		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
	//		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//		0,
	//		0, nullptr,
	//		1, &bufferBarrier,
	//		0, nullptr);
	//}

	{
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, 0);

		vkCmdDispatch(command_buffer, object_count, 1, 1);
	}

	// TODO: need a barrier?
	//if (device.queueFamilyIndices.graphics != device.queueFamilyIndices.compute)
	//{
	//	VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
	//	bufferBarrier.buffer = indirect_command_buffer.buffer;
	//	bufferBarrier.size = indirect_command_buffer.descriptor.range;
	//	bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	//	bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	//	bufferBarrier.srcQueueFamilyIndex = device.queueFamilyIndices.compute;
	//	bufferBarrier.dstQueueFamilyIndex = device.queueFamilyIndices.graphics;

	//	vkCmdPipelineBarrier(
	//		command_buffer,
	//		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
	//		0,
	//		0, nullptr,
	//		1, &bufferBarrier,
	//		0, nullptr);
	//}


	vkEndCommandBuffer(command_buffer);
}

void CullingPipeline::prepare(VkPipelineCache& pipeline_cache, VkDescriptorPool& descriptor_pool, vks::Buffer& uniform_buffer)
{
	// Prepare compute queue
	vkGetDeviceQueue(device, device.queueFamilyIndices.compute, 0, &compute_queue);

	// Descriptor set layout binding
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Instance input data buffer
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_COMPUTE_BIT,
			0),
		// Binding 1: Indirect draw command output buffer (input)
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_COMPUTE_BIT,
			1),
		// Binding 2: Uniform buffer with global matrices (input)
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_COMPUTE_BIT,
			2),
		// Binding 3: Indirect draw stats (output)
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_COMPUTE_BIT,
			3),
	};

	// Descriptor set layout
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

	// update descriptor sets
	std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets =
	{
		// Binding 0: Instance input data buffer
		vks::initializers::writeDescriptorSet(
			descriptor_set,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			0,
			&instance_buffer.descriptor),
		// Binding 1: Indirect draw command output buffer
		vks::initializers::writeDescriptorSet(
			descriptor_set,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			1,
			&indirect_command_buffer.descriptor),
		// Binding 2: Uniform buffer with global matrices
		vks::initializers::writeDescriptorSet(
			descriptor_set,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			2,
			&uniform_buffer.descriptor),
		// Binding 3: Atomic counter (written in shader)
		vks::initializers::writeDescriptorSet(
			descriptor_set,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			3,
			&indircet_draw_count_buffer.descriptor),
	};

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, NULL);

	// Create pipeline
	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipeline_layout, 0);
	computePipelineCreateInfo.stage = loadShader(getAssetPath() + "shaders/glsl/gpudrivenpipeline/culling.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);;

	VK_CHECK_RESULT(vkCreateComputePipelines(device.logicalDevice, pipeline_cache, 1, &computePipelineCreateInfo, nullptr, &pipeline));

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = device.queueFamilyIndices.compute;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device.logicalDevice, &cmdPoolInfo, nullptr, &command_pool));

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

void CullingPipeline::submit()
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

void CullingPipeline::prepareBuffers(VkQueue& queue)
{
	vks::Buffer stagingBuffer;

	std::vector<InstanceData> instance_data(scene.getNodes().size());
	indirect_commands.resize(scene.getNodes().size());

	for (uint32_t i = 0; i < scene.getNodes().size(); i++)
	{
		id_lookup.insert({ scene.getNodes()[i]->getID(), i });
		object_count++;

		indirect_commands[i].instanceCount = 1;
		indirect_commands[i].firstInstance = i;
	}

	indirect_status.draw_count.resize(indirect_commands.size());

	// Transfer indirect command buffer
	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		indirect_commands.size() * sizeof(VkDrawIndexedIndirectCommand),
		indirect_commands.data()));

	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&indirect_command_buffer,
		stagingBuffer.size));

	device.copyBuffer(&stagingBuffer, &indirect_command_buffer, queue);
	stagingBuffer.destroy();

	// indirect draw count buffer
	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&indircet_draw_count_buffer,
		sizeof(uint32_t)*indirect_status.draw_count.size()));

	VK_CHECK_RESULT(indircet_draw_count_buffer.map());

	// Setting instance data
	for (uint32_t i = 0; i < scene.getNodes().size(); i++)
	{
		auto& node = scene.getNodes()[i];

		if (node->hasComponent<chaf::Mesh>())
		{
			auto& mesh_bounds = node->getComponent<chaf::Mesh>().getBounds();
			auto& transform = node->getComponent<chaf::Transform>();

			chaf::AABB world_bounds = { mesh_bounds.getMin(), mesh_bounds.getMax() };
			world_bounds.transform(transform.getWorldMatrix());

			instance_data[i].max = world_bounds.getMax();
			instance_data[i].min = world_bounds.getMin();
		}
	}

	// Transfer instance data
	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		instance_data.size() * sizeof(InstanceData),
		instance_data.data()));

	VK_CHECK_RESULT(device.createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&instance_buffer,
		stagingBuffer.size));

	device.copyBuffer(&stagingBuffer, &instance_buffer, queue);
	stagingBuffer.destroy();
}
