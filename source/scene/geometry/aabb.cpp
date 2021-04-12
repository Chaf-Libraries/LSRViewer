#include <scene/geometry/aabb.h>

namespace chaf
{
	AABB::AABB()
	{
		reset();
	}

	AABB::AABB(const glm::vec3& min, const glm::vec3& max) :
		min{ min },
		max{ max }
	{
	}

	void AABB::update(const glm::vec3& point)
	{
		min = glm::min(min, point);
		max = glm::max(max, point);
	}

	void AABB::update(const std::vector<glm::vec3>& vertex_data, const std::vector<uint16_t>& index_data)
	{
		if (index_data.size() > 0)
		{
			for (size_t index_id = 0; index_id < index_data.size(); index_id++)
			{
				update(vertex_data[index_data[index_id]]);
			}
		}
		else
		{
			for (size_t vertex_id = 0; vertex_id < vertex_data.size(); vertex_id++)
			{
				update(vertex_data[vertex_id]);
			}
		}
	}

	void AABB::transform(const glm::mat4& transform)
	{
		//min = max = glm::vec4(min, 1.0f) * transform;

		glm::vec3 min_ = min;
		glm::vec3 max_ = max;

		reset();

		update(glm::vec4(min_.x, min_.y, max_.z, 1.0f) * transform);
		update(glm::vec4(min_.x, max_.y, min_.z, 1.0f) * transform);
		update(glm::vec4(min_.x, max_.y, max_.z, 1.0f) * transform);
		update(glm::vec4(max_.x, min_.y, min_.z, 1.0f) * transform);
		update(glm::vec4(max_.x, min_.y, max_.z, 1.0f) * transform);
		update(glm::vec4(max_.x, max_.y, min_.z, 1.0f) * transform);
		update(glm::vec4(max_, 1.0f) * transform);
	}

	glm::vec3 AABB::getScale() const
	{
		return max - min;
	}

	glm::vec3 AABB::getCenter() const
	{
		return (max + min) / 2.f;
	}

	glm::vec3 AABB::getMin() const
	{
		return min;
	}

	glm::vec3 AABB::getMax() const
	{
		return max;
	}

	void AABB::reset()
	{
		min = std::numeric_limits<glm::vec3>::max();
		max = -std::numeric_limits<glm::vec3>::max();
	}

	void AABB::set(const glm::vec3& min, const glm::vec3& max)
	{
		this->min = min;
		this->max = max;
	}
}