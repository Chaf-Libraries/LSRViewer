#include <renderer/scene_pipeline.h>

#include <scene/node.h>
#include <scene/components/mesh.h>
#include <scene/components/transform.h>

#ifndef ENABLE_DESCRIPTOR_INDEXING
void ScenePipeline::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, chaf::Frustum& frustum)
{
	if (!node->hasComponent<chaf::Mesh>())return;

	auto& mesh = node->getComponent<chaf::Mesh>();
	auto& transform = node->getComponent<chaf::Transform>();

	if (mesh.getPrimitives().size() > 0) {
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		auto& model = transform.getWorldMatrix();
		auto currentParent = node->getParent();

		for (auto& primitive : mesh.getPrimitives())
		{
			chaf::AABB world_bounds = { primitive.bbox.getMin(), primitive.bbox.getMax() };

			world_bounds.transform(transform.getWorldMatrix());

			// CPU culling here
			if (primitive.index_count > 0 && frustum.checkAABB(world_bounds))
			{
				primitive.visible = true;
				auto& material = node->getScene().materials[primitive.material_index];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
#ifdef USE_TESSELLATION
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
#else
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
#endif // USE_TESSELLATION
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.index_count, 1, primitive.first_index, 0, 0);
			}
			else
			{
				primitive.visible = false;
			}
		}
	}
	for (auto& child : node->getChildren())
	{
		drawNode(commandBuffer, pipelineLayout, child, frustum);
	}
}

// Indirect draw
void ScenePipeline::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, CullingPipeline& culling_pipeline)
{
	if (!node->hasComponent<chaf::Mesh>())return;

	auto& mesh = node->getComponent<chaf::Mesh>();
	auto& transform = node->getComponent<chaf::Transform>();
	
	if (mesh.getPrimitives().size() > 0)
	{
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		auto& model = transform.getWorldMatrix();
		auto currentParent = node->getParent();
		
		for (uint32_t i = 0; i < mesh.getPrimitives().size(); i++)
		{
			auto& primitive = mesh.getPrimitives()[i];

			if (primitive.index_count > 0)
			{
				auto& material = node->getScene().materials[primitive.material_index];

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
#ifdef USE_TESSELLATION
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
#else
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
#endif // USE_TESSELLATION

				
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);

#ifdef USE_OCCLUSION_QUERY
				vkCmdBeginQuery(commandBuffer, culling_pipeline.query_pool, culling_pipeline.id_lookup[node->getID()][i], VK_FLAGS_NONE);
#endif // USE_OCCLUSION_QUERY

				vkCmdDrawIndexedIndirect(commandBuffer, culling_pipeline.indirect_command_buffer.buffer, culling_pipeline.id_lookup[node->getID()][i] * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));

#ifdef USE_OCCLUSION_QUERY
				vkCmdEndQuery(commandBuffer, culling_pipeline.query_pool, culling_pipeline.id_lookup[node->getID()][i]);
#endif // USE_OCCLUSION_QUERY				
			}
		}
	}
}
#else
void ScenePipeline::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, chaf::Frustum& frustum)
{
	if (!node->hasComponent<chaf::Mesh>())return;

	auto& mesh = node->getComponent<chaf::Mesh>();
	auto& transform = node->getComponent<chaf::Transform>();

	if (mesh.getPrimitives().size() > 0) {
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		auto& model = transform.getWorldMatrix();
		auto currentParent = node->getParent();

		for (auto& primitive : mesh.getPrimitives())
		{
			chaf::AABB world_bounds = { primitive.bbox.getMin(), primitive.bbox.getMax() };

			world_bounds.transform(transform.getWorldMatrix());

			// CPU culling here
			if (primitive.index_count > 0 && frustum.checkAABB(world_bounds))
			{
				primitive.visible = true;
				auto& material = node->getScene().materials[primitive.material_index];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);

#ifdef USE_TESSELLATION
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
#else
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
#endif // USE_TESSELLATION
#ifdef ENABLE_DESCRIPTOR_INDEXING
				std::vector<uint32_t> texture_index = { material.baseColorTextureIndex, material.normalTextureIndex };
#ifdef USE_TESSELLATION
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t) * 2, texture_index.data());
#else
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t) * 2, texture_index.data());
#endif // USE_TESSELLATION
#endif // ENABLE_DESCRIPTOR_INDEXING

				vkCmdDrawIndexed(commandBuffer, primitive.index_count, 1, primitive.first_index, 0, 0);
			}
			else
			{
				primitive.visible = false;
			}
		}
	}
	for (auto& child : node->getChildren())
	{
		drawNode(commandBuffer, pipelineLayout, child, frustum);
	}
}

// Indirect draw
void ScenePipeline::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, CullingPipeline& culling_pipeline)
{
	if (!node->hasComponent<chaf::Mesh>())return;

	auto& mesh = node->getComponent<chaf::Mesh>();
	auto& transform = node->getComponent<chaf::Transform>();

	if (mesh.getPrimitives().size() > 0)
	{
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		auto model = transform.getWorldMatrix();
		auto currentParent = node->getParent();

		for (uint32_t i = 0; i < mesh.getPrimitives().size(); i++)
		{
			auto& primitive = mesh.getPrimitives()[i];

			if (primitive.index_count > 0)
			{
				auto& material = node->getScene().materials[primitive.material_index];

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);

#ifdef USE_TESSELLATION
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
#else
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
#endif // USE_TESSELLATION
#ifdef ENABLE_DESCRIPTOR_INDEXING
				std::vector<uint32_t> texture_index = { material.baseColorTextureIndex, material.normalTextureIndex };
#ifdef USE_TESSELLATION
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t) * 2, texture_index.data());
#else
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t) * 2, texture_index.data());
#endif // USE_TESSELLATION
#endif // ENABLE_DESCRIPTOR_INDEXING
				vkCmdDrawIndexedIndirect(commandBuffer, culling_pipeline.indirect_command_buffer.buffer, culling_pipeline.id_lookup[node->getID()][i] * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));		
			}
		}
	}
}


#endif

ScenePipeline::ScenePipeline(vks::VulkanDevice& device, chaf::Scene& scene) :
	chaf::PipelineBase{ device }, scene{ scene }
{
}

ScenePipeline::~ScenePipeline()
{
	vkDestroyPipelineLayout(device.logicalDevice, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayouts.matrices, nullptr);
	vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayouts.textures, nullptr);
	vkDestroyDescriptorPool(device.logicalDevice, descriptor_pool, nullptr);

	// Destroy depth image
	vkDestroySampler(device, depth_image.sampler, nullptr);
	vkDestroyImageView(device, depth_image.view, nullptr);
	vkDestroyImage(device, depth_image.image, nullptr);
	vkFreeMemory(device, depth_image.mem, nullptr);
}

#ifndef ENABLE_DESCRIPTOR_INDEXING
void ScenePipeline::bindCommandBuffers(VkCommandBuffer& cmd_buffer, chaf::Frustum& frustum)
{
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	VkDeviceSize offsets[1] = { 0 };

	if (scene.buffer_cacher->hasVBO(0) && scene.buffer_cacher->hasEBO(0))
	{
		vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &scene.buffer_cacher->getVBO(0).buffer, offsets);
		vkCmdBindIndexBuffer(cmd_buffer, scene.buffer_cacher->getEBO(0).buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	// Render all nodes at top-level
	for (auto& node : scene.getNodes())
	{
		if (node->hasComponent<chaf::Mesh>())
		{
			// CPU culling
			drawNode(cmd_buffer, pipelineLayout, &*node, frustum);
		}
	}
}

void ScenePipeline::bindCommandBuffers(VkCommandBuffer& cmd_buffer, CullingPipeline& culling_pipeline)
{
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	VkDeviceSize offsets[1] = { 0 };

	if (scene.buffer_cacher->hasVBO(0) && scene.buffer_cacher->hasEBO(0))
	{
		vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &scene.buffer_cacher->getVBO(0).buffer, offsets);
		vkCmdBindIndexBuffer(cmd_buffer, scene.buffer_cacher->getEBO(0).buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	for (auto& node : scene.getNodes())
	{
		if (node->hasComponent<chaf::Mesh>())
		{
			// GPU culling
			if (scene.buffer_cacher->hasVBO(0) && scene.buffer_cacher->hasEBO(0))
			{
				drawNode(cmd_buffer, pipelineLayout, &*node, culling_pipeline);
			}
		}
	}
}
#else
void ScenePipeline::bindCommandBuffers(VkCommandBuffer& cmd_buffer, chaf::Frustum& frustum)
{
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	VkDeviceSize offsets[1] = { 0 };

	if (scene.buffer_cacher->hasVBO(0) && scene.buffer_cacher->hasEBO(0))
	{
		vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &scene.buffer_cacher->getVBO(0).buffer, offsets);
		vkCmdBindIndexBuffer(cmd_buffer, scene.buffer_cacher->getEBO(0).buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	// Render all nodes at top-level
	for (auto& node : scene.getNodes())
	{
		if (node->hasComponent<chaf::Mesh>())
		{
			// CPU culling
			drawNode(cmd_buffer, pipelineLayout, &*node, frustum);
		}
	}
}

void ScenePipeline::bindCommandBuffers(VkCommandBuffer& cmd_buffer, CullingPipeline& culling_pipeline)
{
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &scene.bindless_descriptor_set, 0, nullptr);

	VkDeviceSize offsets[1] = { 0 };

	if (scene.buffer_cacher->hasVBO(0) && scene.buffer_cacher->hasEBO(0))
	{
		vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &scene.buffer_cacher->getVBO(0).buffer, offsets);
		vkCmdBindIndexBuffer(cmd_buffer, scene.buffer_cacher->getEBO(0).buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	for (auto& node : scene.getNodes())
	{
		if (node->hasComponent<chaf::Mesh>())
		{
			// GPU culling
			if (scene.buffer_cacher->hasVBO(0) && scene.buffer_cacher->hasEBO(0))
			{
				drawNode(cmd_buffer, pipelineLayout, &*node, culling_pipeline);
			}
		}
	}
}
#endif // ENABLE_DESCRIPTOR_INDEXING

#ifdef ENABLE_DESCRIPTOR_INDEXING
void ScenePipeline::setupDescriptors(vks::Buffer& uniform_buffer)
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(scene.images.size()))
	};

	const uint32_t maxSetCount = static_cast<uint32_t>(scene.images.size());
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptor_pool));

	// Descriptor set layout for passing matrices
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 0)
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));

	// Descriptor set layout for passing material textures
	setLayoutBindings = {
		// binding all textures
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<uint32_t>(scene.images.size()))
	};
	descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
	descriptorSetLayoutCI.bindingCount = 1;

	// The fragment shader will be using an unsized array of samplers, which has to be marked with the VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{};
	setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	setLayoutBindingFlags.bindingCount = 1;
	std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags = {
		0,
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
	};
	setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();
	descriptorSetLayoutCI.pNext = &setLayoutBindingFlags;

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));

	// Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material)
	std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	// We will use push constants to push the local matrices of a primitive to the vertex shader

	std::vector<VkPushConstantRange> pushConstantRanges = {
		
#ifdef USE_TESSELLATION
		vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, sizeof(glm::mat4), 0),
		vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(uint32_t) * 2, sizeof(glm::mat4))
#else
		vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0),
		vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(uint32_t) * 2, sizeof(glm::mat4))
#endif // USE_TESSELLATION
	};
	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
	pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	// Descriptor set for scene matrices
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &descriptorSetLayouts.matrices, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
	VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniform_buffer.descriptor);
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

	VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo = {};

	uint32_t variableDescCounts[] = { static_cast<uint32_t>(scene.textures.size()) };

	variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
	variableDescriptorCountAllocInfo.descriptorSetCount = 1;
	variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

	// Descriptor set for bindless texture
	allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &descriptorSetLayouts.textures, 1);
	allocInfo.pNext = &variableDescriptorCountAllocInfo;

	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &scene.bindless_descriptor_set));

	std::vector<VkDescriptorImageInfo> textureDescriptors(scene.images.size());
	for (size_t i = 0; i < scene.images.size(); i++) 
	{
		textureDescriptors[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textureDescriptors[i].sampler = scene.images[i].texture.sampler;
		textureDescriptors[i].imageView = scene.images[i].texture.view;
	}

	writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstBinding = 0;
	writeDescriptorSet.dstArrayElement = 0;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet.descriptorCount = static_cast<uint32_t>(scene.images.size());
	writeDescriptorSet.pBufferInfo = 0;
	writeDescriptorSet.dstSet = scene.bindless_descriptor_set;
	writeDescriptorSet.pImageInfo = textureDescriptors.data();

	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}

#else
void ScenePipeline::setupDescriptors(vks::Buffer& uniform_buffer)
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(scene.materials.size()) * 3)
	};

	const uint32_t maxSetCount = static_cast<uint32_t>(scene.images.size());
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptor_pool));

	// Descriptor set layout for passing matrices
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 0)
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));

	// Descriptor set layout for passing material textures
	setLayoutBindings = {
		// Color map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		// Normal map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	};
	descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
	descriptorSetLayoutCI.bindingCount = 2;
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));

	// Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material)
	std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	// We will use push constants to push the local matrices of a primitive to the vertex shader
#ifdef USE_TESSELLATION
	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
#else
	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
#endif // USE_TESSELLATION

	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;

	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	// Descriptor set for scene matrices
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &descriptorSetLayouts.matrices, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
	VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniform_buffer.descriptor);
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

	// Descriptor sets for materials
	for (auto& material : scene.materials) {
		const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptor_pool, &descriptorSetLayouts.textures, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &material.descriptorSet));

		VkDescriptorImageInfo colorMap = scene.images[material.baseColorTextureIndex].texture.descriptor;
		VkDescriptorImageInfo normalMap = scene.images[material.normalTextureIndex].texture.descriptor;

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &colorMap),
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalMap),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void ScenePipeline::setupDescriptors(VkDescriptorPool& descriptorPool, vks::Buffer& uniform_buffer, HizPipeline& hiz_pipeline)
{
	// Descriptor set layout for passing matrices
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));

	// Descriptor set layout for passing material textures
	setLayoutBindings = {
		// Color map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		// Normal map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		// Depth map
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
	};
	descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
	descriptorSetLayoutCI.bindingCount = 3;
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device.logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));

	// Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material)
	std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	// We will use push constants to push the local matrices of a primitive to the vertex shader
	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	// Descriptor set for scene matrices
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
	VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniform_buffer.descriptor);
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

	// Descriptor sets for materials
	for (auto& material : scene.materials) {
		allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &material.descriptorSet));

		VkDescriptorImageInfo colorMap = scene.images[material.baseColorTextureIndex].texture.descriptor;
		VkDescriptorImageInfo normalMap = scene.images[material.normalTextureIndex].texture.descriptor;

		VkDescriptorImageInfo dstTarget;
		//dstTarget.sampler = hiz_pipeline.hiz_image.sampler;
		//dstTarget.imageView = hiz_pipeline.hiz_image.views[0];
		//dstTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		dstTarget=depth_image.descriptor;

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &colorMap),
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalMap),
			vks::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &dstTarget),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}
#endif

void ScenePipeline::preparePipelines(VkPipelineCache& pipeline_cache, VkRenderPass& render_pass)
{
#ifdef USE_TESSELLATION
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 0, VK_FALSE);
#else
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
#endif // USE_TESSELLATION

	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
#ifdef USE_TESSELLATION
	VkPipelineTessellationStateCreateInfo tessellationStateCI = vks::initializers::pipelineTessellationStateCreateInfo(3);
	std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages;
#else
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
#endif

	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		vks::initializers::vertexInputBindingDescription(0, sizeof(chaf::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, pos)),
		vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, normal)),
		vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, uv)),
		vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, color)),
		vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(chaf::Vertex, tangent)),
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);

	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, render_pass, 0);
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
#ifdef USE_TESSELLATION
	pipelineCI.pTessellationState = &tessellationStateCI;
#endif
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

#ifdef USE_TESSELLATION
#ifdef ENABLE_DESCRIPTOR_INDEXING
	shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_indexing_tes.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_indexing_tes.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	shaderStages[2] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_indexing_tes.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	shaderStages[3] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_indexing_tes.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
#else
	shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_tes.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_tes.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	shaderStages[2] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_tes.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	shaderStages[3] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_tes.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
#endif // ENABLE_DESCRIPTOR_INDEXING
#else
#ifdef ENABLE_DESCRIPTOR_INDEXING
	shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_indexing.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene_indexing.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
#else
	shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/spirv/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
#endif // ENABLE_DESCRIPTOR_INDEXING

#endif // USE_TESSELLATION

	for (auto& material : scene.materials)
	{

		struct MaterialSpecializationData
		{
			bool alphaMask;
			float alphaMaskCutoff;
		} materialSpecializationData;

		materialSpecializationData.alphaMask = material.alphaMode == "MASK";
		materialSpecializationData.alphaMaskCutoff = material.alphaCutOff;

		// POI: Constant fragment shader material parameters will be set using specialization constants
		std::vector<VkSpecializationMapEntry> specializationMapEntries = {
			vks::initializers::specializationMapEntry(0, offsetof(MaterialSpecializationData, alphaMask), sizeof(MaterialSpecializationData::alphaMask)),
			vks::initializers::specializationMapEntry(1, offsetof(MaterialSpecializationData, alphaMaskCutoff), sizeof(MaterialSpecializationData::alphaMaskCutoff)),
		};
		VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(specializationMapEntries, sizeof(materialSpecializationData), &materialSpecializationData);
		shaderStages[1].pSpecializationInfo = &specializationInfo;

		// For double sided materials, culling will be disabled
		rasterizationStateCI.cullMode = material.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device.logicalDevice, pipeline_cache, 1, &pipelineCI, nullptr, &material.pipeline));
	}
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

#ifdef ENABLE_DEFERRED
// TODO: beta - deferred rendering
void ScenePipeline::createAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment* attachment, uint32_t width, uint32_t height)
{
	VkImageAspectFlags aspectMask = 0;
	VkImageLayout imageLayout;

	attachment->format = format;

	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	assert(aspectMask > 0);

	VkImageCreateInfo image = vks::initializers::imageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = format;
	image.extent.width = width;
	image.extent.height = height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

	VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &attachment->image));
	vkGetImageMemoryRequirements(device, attachment->image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = device.getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device.logicalDevice, &memAlloc, nullptr, &attachment->memory));
	VK_CHECK_RESULT(vkBindImageMemory(device.logicalDevice, attachment->image, attachment->memory, 0));

	VkImageViewCreateInfo imageView = vks::initializers::imageViewCreateInfo();
	imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageView.format = format;
	imageView.subresourceRange = {};
	imageView.subresourceRange.aspectMask = aspectMask;
	imageView.subresourceRange.baseMipLevel = 0;
	imageView.subresourceRange.levelCount = 1;
	imageView.subresourceRange.baseArrayLayer = 0;
	imageView.subresourceRange.layerCount = 1;
	imageView.image = attachment->image;
	VK_CHECK_RESULT(vkCreateImageView(device, &imageView, nullptr, &attachment->view));
}

void ScenePipeline::prepareDeferredFramebuffer()
{
	// Color attachments

	// Virtual texture read back
	createAttachment(
		VK_FORMAT_R32G32B32A32_UINT,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		&deferred_pipeline.readback, 200, 200);

	// Depth attachment

	// Find a suitable depth format
	VkFormat attDepthFormat;
	VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(device.physicalDevice, &attDepthFormat);
	assert(validDepthFormat);

	createAttachment(
		attDepthFormat,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		&deferred_pipeline.depth, 200, 200);

	// Set up separate renderpass with references to the color and depth attachments
	std::array<VkAttachmentDescription, 2> attachmentDescs = {};

	// Init attachment properties
	for (uint32_t i = 0; i < 2; ++i)
	{
		attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 1)
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	// Formats
	attachmentDescs[0].format = deferred_pipeline.readback.format;
	attachmentDescs[1].format = deferred_pipeline.depth.format;

	VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	
	VkAttachmentReference depthReference = { 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments = &colorReference;
	subpass.colorAttachmentCount = 1;
	subpass.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for attachment layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pAttachments = attachmentDescs.data();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &deferred_pipeline.render_pass));

	std::array<VkImageView, 2> attachments;
	attachments[0] = deferred_pipeline.readback.view;
	attachments[1] = deferred_pipeline.depth.view;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.pNext = NULL;
	fbufCreateInfo.renderPass = deferred_pipeline.render_pass;
	fbufCreateInfo.pAttachments = attachments.data();
	fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbufCreateInfo.width = 200;
	fbufCreateInfo.height = 200;
	fbufCreateInfo.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &deferred_pipeline.frame_buffer));

	// Create sampler to sample from the color attachments
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
	VK_CHECK_RESULT(vkCreateSampler(device.logicalDevice, &sampler, nullptr, &deferred_pipeline.sampler));
}
#endif
