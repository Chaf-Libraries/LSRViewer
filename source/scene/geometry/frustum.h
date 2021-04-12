#pragma once

#include <array>

#include <glm/glm.hpp>

namespace chaf
{
	class AABB;

	class Frustum
	{
	public:
		Frustum() = default;

		Frustum(glm::mat4 matrix);

		~Frustum() = default;

		enum PlaneSide
		{
			Left = 0,
			Right = 1,
			Top = 2,
			Bottom = 3,
			Back = 4,
			Front = 5
		};

		std::array<glm::vec4, 6> planes;

		void update(glm::mat4 matrix);

		bool checkSphere(glm::vec3 pos, float radius);

		bool checkAABB(const AABB& aabb);
	};
}