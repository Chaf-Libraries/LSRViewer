#include <renderer/scene_pipeline.h>

#include <scene/node.h>
#include <scene/components/mesh.h>
#include <scene/components/transform.h>

inline void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node)
{
	if (!node->hasComponent<chaf::Mesh>())return;

	auto& mesh = node->getComponent<chaf::Mesh>();
	auto& transform = node->getComponent<chaf::Transform>();
		
	if (mesh.getPrimitives().size() > 0) {
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		glm::mat4 nodeMatrix = transform.getWorldMatrix();
		auto currentParent = node->getParent();

		// Pass the final matrix to the vertex shader using push constants
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
		for (auto& primitive : mesh.getPrimitives()) {
			if (primitive.index_count > 0) {
				auto& material = node->getScene().materials[primitive.material_index];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.index_count, 1, primitive.first_index, 0, 0);
			}
		}
	}
	for (auto& child : node->getChildren())
	{
		drawNode(commandBuffer, pipelineLayout, child);
	}
}

// TODO: Indirect draw
inline void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, vks::Buffer& indircet_command_buffer, uint32_t index)
{
	if (!node->hasComponent<chaf::Mesh>())return;

	auto& mesh = node->getComponent<chaf::Mesh>();
	auto& transform = node->getComponent<chaf::Transform>();

	if (mesh.getPrimitives().size() > 0)
	{
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		glm::mat4 nodeMatrix = transform.getWorldMatrix();
		auto currentParent = node->getParent();

		// Pass the final matrix to the vertex shader using push constants
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
		for (auto& primitive : mesh.getPrimitives()) {
			if (primitive.index_count > 0) {
				auto& material = node->getScene().materials[primitive.material_index];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);
				//vkCmdDrawIndexedIndirect(commandBuffer, indircet_command_buffer.buffer, index * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
				vkCmdDrawIndexed(commandBuffer, primitive.index_count, 1, primitive.first_index, 0, 0);
			}
		}
	}
}

ScenePipeline::ScenePipeline(vks::VulkanDevice& device, chaf::Scene& scene) :
	chaf::PipelineBase{ device }, scene{ scene }
{

}

ScenePipeline::~ScenePipeline()
{
	vkDestroyPipelineLayout(device.logicalDevice, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayouts.matrices, nullptr);
	vkDestroyDescriptorSetLayout(device.logicalDevice, descriptorSetLayouts.textures, nullptr);
}

void ScenePipeline::bindCommandBuffers(VkCommandBuffer& cmd_buffer, Camera& camera, chaf::Frustum& frustum)
{
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &scene.buffer_cacher->getVBO(0).buffer, offsets);
	vkCmdBindIndexBuffer(cmd_buffer, scene.buffer_cacher->getEBO(0).buffer, 0, VK_INDEX_TYPE_UINT32);

	// Render all nodes at top-level
	for (auto& node : scene.getNodes())
	{
		if (node->hasComponent<chaf::Mesh>())
		{
			auto& mesh_bounds = node->getComponent<chaf::Mesh>().getBounds();
			auto& transform = node->getComponent<chaf::Transform>();

			chaf::AABB world_bounds = { mesh_bounds.getMin(), mesh_bounds.getMax() };

			world_bounds.transform(transform.getWorldMatrix());

			float distance = glm::length(camera.position - world_bounds.getCenter());
			glm::vec3 center = mesh_bounds.getCenter() + transform.getTranslation();
			float radius = glm::length(mesh_bounds.getScale() * transform.getScale() * 0.5f);
			float world_radius = glm::length(world_bounds.getScale() * 0.5f);

			// CPU culling here
			if (frustum.checkAABB(world_bounds) && distance - radius<camera.getFarClip() && distance + radius>camera.getNearClip())
			{
				node->visibility = true;
				drawNode(cmd_buffer, pipelineLayout, &*node);
			}
			else
			{
				node->visibility = false;
			}
		}
	}
}

void ScenePipeline::setupDescriptors(VkDescriptorPool& descriptorPool, vks::Buffer& uniform_buffer)
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
	};
	descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
	descriptorSetLayoutCI.bindingCount = 2;
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
		const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
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

void ScenePipeline::preparePipelines(VkPipelineCache& pipeline_cache, VkRenderPass& render_pass)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

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
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	shaderStages[0] = loadShader("../data/shaders/glsl/gpudrivenpipeline/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader("../data/shaders/glsl/gpudrivenpipeline/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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
