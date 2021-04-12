#include <scene/geometry/frustum.h>
#include <scene/geometry/aabb.h>

namespace chaf
{
	Frustum::Frustum(glm::mat4 matrix)
	{
		update(matrix);
	}

	void Frustum::update(glm::mat4 matrix)
	{
		planes[PlaneSide::Left].x = matrix[0].w + matrix[0].x;
		planes[PlaneSide::Left].y = matrix[1].w + matrix[1].x;
		planes[PlaneSide::Left].z = matrix[2].w + matrix[2].x;
		planes[PlaneSide::Left].w = matrix[3].w + matrix[3].x;

		planes[PlaneSide::Right].x = matrix[0].w - matrix[0].x;
		planes[PlaneSide::Right].y = matrix[1].w - matrix[1].x;
		planes[PlaneSide::Right].z = matrix[2].w - matrix[2].x;
		planes[PlaneSide::Right].w = matrix[3].w - matrix[3].x;

		planes[PlaneSide::Top].x = matrix[0].w - matrix[0].y;
		planes[PlaneSide::Top].y = matrix[1].w - matrix[1].y;
		planes[PlaneSide::Top].z = matrix[2].w - matrix[2].y;
		planes[PlaneSide::Top].w = matrix[3].w - matrix[3].y;

		planes[PlaneSide::Bottom].x = matrix[0].w + matrix[0].y;
		planes[PlaneSide::Bottom].y = matrix[1].w + matrix[1].y;
		planes[PlaneSide::Bottom].z = matrix[2].w + matrix[2].y;
		planes[PlaneSide::Bottom].w = matrix[3].w + matrix[3].y;

		planes[PlaneSide::Back].x = matrix[0].w + matrix[0].z;
		planes[PlaneSide::Back].y = matrix[1].w + matrix[1].z;
		planes[PlaneSide::Back].z = matrix[2].w + matrix[2].z;
		planes[PlaneSide::Back].w = matrix[3].w + matrix[3].z;

		planes[PlaneSide::Front].x = matrix[0].w - matrix[0].z;
		planes[PlaneSide::Front].y = matrix[1].w - matrix[1].z;
		planes[PlaneSide::Front].z = matrix[2].w - matrix[2].z;
		planes[PlaneSide::Front].w = matrix[3].w - matrix[3].z;

		for (uint32_t i = 0; i < planes.size(); i++)
		{
			planes[i] /= sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
		}
	}

	bool Frustum::checkSphere(glm::vec3 pos, float radius)
	{
		for (uint32_t i = 0; i < planes.size(); i++)
		{
			if ((planes[i].x * pos.x) + (planes[i].y * pos.y) + (planes[i].z * pos.z) + planes[i].w + radius<= 0)
			{
				return false;
			}
		}
		return true;
	}

	bool Frustum::checkAABB(const AABB& aabb)
	{
		// Check bounding sphere first
		glm::vec3 pos = aabb.getCenter();
		float radius = glm::length(aabb.getScale()) * 0.5f;
		//return checkSphere(pos, radius);
		if (checkSphere(pos, radius))
		{
			return true;
		}
		// Check AABB
		else
		{
			for (size_t i=0;i<6;i++)
			{
				auto plane = planes[i];
				glm::vec3 plane_normal = { plane.x, plane.y,plane.z };
				float plane_constant = plane.w;

				glm::vec3 axis_vert{};

				// x-axis
				axis_vert.x = plane.x < 0.f ? aabb.getMin().x : aabb.getMax().x;

				// y-axis
				axis_vert.y = plane.y < 0.f ? aabb.getMin().y : aabb.getMax().y;

				// z-axis
				axis_vert.z = plane.z < 0.f ? aabb.getMin().z : aabb.getMax().z;

				if (glm::dot(axis_vert, plane_normal) + plane_constant < 0.f)
				{
					return false;
				}
			}
		}
		return true;
	}
}