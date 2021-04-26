#pragma once

#include <glm/glm.hpp>

#include <string>

#include <vulkan/vulkan.h>

// TODO: PBR Material
// TODO: More Texture Format: ktx, stb, astc...

namespace chaf
{
	struct Material 
	{
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
		uint32_t normalTextureIndex;
		std::string alphaMode = "OPAQUE";
		float alphaCutOff;
		bool doubleSided = false;
		VkDescriptorSet descriptorSet;
		VkPipeline pipeline;
	};
}