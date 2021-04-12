#pragma once

#include <vector>
#include <limits>

#include <glm/glm.hpp>

#undef max
#undef min

namespace chaf
{
	class AABB
	{
	public:
		AABB();

		AABB(const glm::vec3& min, const glm::vec3& max);

		~AABB() = default;

		void update(const glm::vec3& point);

		void update(const std::vector<glm::vec3>& vertex_data, const std::vector<uint16_t>& index_data);

		void transform(const glm::mat4& transform);

		glm::vec3 getScale() const;

		glm::vec3 getCenter() const;

		glm::vec3 getMin() const;

		glm::vec3 getMax() const;

		void reset();

		void set(const glm::vec3& min, const glm::vec3& max);

	private:
		glm::vec3 min{ std::numeric_limits<float>::max() };
		glm::vec3 max{ -std::numeric_limits<float>::max() };
	};
}