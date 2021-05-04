#pragma once

#include <glm/glm.hpp>

#include <string>

#include <VulkanBuffer.h>

// TODO: PBR Material
// TODO: More Texture Format: ktx, stb, astc...

namespace chaf
{
	struct Material 
	{
		// Texture
		struct
		{
			int32_t baseColorTextureIndex = -1;
			int32_t normalTextureIndex = -1;
			int32_t emissiveTextureIndex = -1;
			int32_t occlusionTextureIndex = -1;

			int32_t metallicRoughnessTextureIndex = -1;
			float metallicFactor = 1.f;
			float roughnessFactor = 1.f;
			uint32_t alphaMode = 0; // 0: OPAQUE, 1: MASK

			float alphaCutOff = 0.5f;
			uint32_t doubleSided = 0;

			alignas(16) glm::vec4 baseColorFactor = glm::vec4(1.0f);
			alignas(16)glm::vec3 emissiveFactor = glm::vec3(1.0f);
		}value;
	};
}