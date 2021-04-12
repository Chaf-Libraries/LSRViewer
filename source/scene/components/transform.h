#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>

#include <scene/scene.h>
#include <scene/node.h>

namespace chaf
{
	class Transform
	{
	public:
		Transform(Node* node);

		~Transform() = default;

		Node& getNode();

		void setTranslation(const glm::vec3& translation);

		void setRotation(const glm::quat& rotation);

		void setScale (const glm::vec3& scale);

		void setMatrix(const glm::mat4& matrix);

		const glm::vec3& getTranslation() const;

		const glm::quat& getRotation() const;

		const glm::vec3& getScale() const;

		glm::mat4 getMatrix() const;

		glm::mat4 getWorldMatrix();

	private:
		Node* node;

		glm::vec3 translation{ glm::vec3(0.f) };
		
		glm::vec3 scale{ glm::vec3(1.f) };
		
		glm::quat rotation{ glm::quat(1.f,0.f,0.f,0.f) };

		glm::mat4 world_matrix{ glm::mat4(1.f) };

		bool is_world_matrix_update{ false };
	};
}