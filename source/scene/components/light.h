#pragma once

#include <glm/glm.hpp>

namespace chaf
{
	enum class LightType
	{
		Point,
		Spot,
		Directional
	};

	struct LightProperties
	{
		glm::vec3 direction{ 0.f, 0.f, -1.f };
		glm::vec3 color{ 1.f, 1.f, 1.f };
		float intensity{ 1.f };
		float range{ 0.f };
		float inner_cone_angle{ 0.f };
		float outer_cone_angle{ 0.f };
	};

	class Light
	{
	public:
		Light() = default;

		~Light() = default;

		LightType getType() const;

		void setType(LightType type);

		const LightProperties& getProperties() const;

		void setProperties(LightProperties& properties);

	private:
		LightType type;
		LightProperties properties;
	};
}