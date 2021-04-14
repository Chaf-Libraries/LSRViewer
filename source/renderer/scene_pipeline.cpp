#include <renderer/scene_pipeline.h>

#include <scene/node.h>
#include <scene/components/mesh.h>
#include <scene/components/transform.h>

inline void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, chaf::Frustum& frustum)
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

//////////////////////////

bool checkSphere(glm::vec4 frustum[], glm::vec3 pos, float radius)
{
	for (glm::uint i = 0; i < 6; i++)
	{
		if ((frustum[i].x * pos.x) + (frustum[i].y * pos.y) + (frustum[i].z * pos.z) + frustum[i].w + radius <= 0.0)
		{
			return false;
		}
	}
	return true;
}

bool checkAABB(glm::vec4 frustum[], glm::vec3 min_val, glm::vec3 max_val)
{
	glm::vec3 pos = (min_val + max_val) * 0.5f;
	float radius = glm::length(max_val - min_val) * 0.5f;

	if (checkSphere(frustum, pos, radius))
	{
		return true;
	}
	else
	{
		for (glm::uint i = 0; i < 6; i++)
		{
			glm::vec4 plane = frustum[i];
			glm::vec3 plane_normal = { plane.x, plane.y, plane.z };
			float plane_constant = plane.w;

			glm::vec3 axis_vert = { 0.0, 0.0, 0.0 };

			// x-axis
			axis_vert.x = plane.x < 0.0 ? min_val.x : max_val.x;

			// y-axis
			axis_vert.y = plane.y < 0.0 ? min_val.y : max_val.y;

			// z-axis
			axis_vert.z = plane.z < 0.0 ? min_val.z : max_val.z;

			if (dot(axis_vert, plane_normal) + plane_constant < 0.0f)
			{
				return false;
			}
		}
	}
	return true;
}

/// ///////////////////


inline void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, glm::vec4 frustum[], CullingPipeline& culling_pipeline)
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
		for (uint32_t i = 0; i < mesh.getPrimitives().size(); i++)
		{
			auto& primitive = mesh.getPrimitives()[i];

			auto& instance_data = culling_pipeline.instance_data[culling_pipeline.id_lookup[node->getID()][i]];

			// CPU culling here
			if (checkAABB(frustum, instance_data.min, instance_data.max))
			{
				primitive.visible = true;
				auto& material = node->getScene().materials[primitive.material_index];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);

				auto& cmd = culling_pipeline.indirect_commands[culling_pipeline.id_lookup[node->getID()][i]];
				//vkCmdDrawIndexed(commandBuffer, primitive.index_count, 1, primitive.first_index, 0, 0);
				vkCmdDrawIndexed(commandBuffer, cmd.indexCount, cmd.instanceCount, cmd.firstIndex, cmd.vertexOffset, cmd.firstInstance);
			}
			else
			{
				primitive.visible = false;
			}
		}
	}
	for (auto& child : node->getChildren())
	{
		drawNode(commandBuffer, pipelineLayout, child, frustum, culling_pipeline);
	}
}

// TODO: Indirect draw
inline void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, chaf::Node* node, CullingPipeline& culling_pipeline)
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
		for (uint32_t i = 0; i < mesh.getPrimitives().size(); i++)
		{
			auto& primitive = mesh.getPrimitives()[i];

			if (primitive.index_count > 0) 
			{
				auto& material = node->getScene().materials[primitive.material_index];

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);
				vkCmdDrawIndexedIndirect(commandBuffer, culling_pipeline.indirect_command_buffer.buffer, culling_pipeline.id_lookup[node->getID()][i] * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
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

void ScenePipeline::bindCommandBuffers(VkCommandBuffer& cmd_buffer, chaf::Frustum& frustum)
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
			// CPU culling
			drawNode(cmd_buffer, pipelineLayout, &*node, frustum);
		}
	}
}

void ScenePipeline::bindCommandBuffers(VkCommandBuffer& cmd_buffer, CullingPipeline& culling_pipeline)
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
			// GPU culling
			drawNode(cmd_buffer, pipelineLayout, &*node, culling_pipeline);
		}
	}
}

void ScenePipeline::bindCommandBuffers(VkCommandBuffer& cmd_buffer, glm::vec4 frustum[], CullingPipeline& culling_pipeline)
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
			// GPU culling
			drawNode(cmd_buffer, pipelineLayout, &*node, frustum, culling_pipeline);
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
