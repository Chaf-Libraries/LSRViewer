#pragma once

#include <glm/glm.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>

#include <scene/geometry/aabb.h>

namespace chaf
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
		glm::vec4 tangent;
	};

	struct VertexBuffer
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	struct IndexBuffer
	{
		int32_t count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	struct Primitive
	{
		AABB bbox;
		uint32_t first_index;
		uint32_t index_count;
		int32_t material_index;
		size_t id;

		void updateID();
	};
}