#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <scene/components/transform.h>

#include <glm/gtx/matrix_decompose.hpp>

namespace chaf
{
	Transform::Transform(Node* node) :
		node{ node }
	{

	}

	Node& Transform::getNode()
	{
		return *node;
	}

	void Transform::setTranslation(const glm::vec3& translation)
	{
		this->translation = translation;
	}

	void Transform::setRotation(const glm::quat& rotation)
	{
		this->rotation = rotation;
	}

	void Transform::setScale(const glm::vec3& scale)
	{
		this->scale = scale;
	}

	void Transform::setMatrix(const glm::mat4& matrix)
	{
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(matrix, scale, rotation, translation, skew, perspective);
		rotation = glm::conjugate(rotation);

		is_world_matrix_update = true;
	}

	const glm::vec3& Transform::getTranslation() const
	{
		return translation;
	}

	const glm::quat& Transform::getRotation() const
	{
		return rotation;
	}

	const glm::vec3& Transform::getScale() const
	{
		return scale;
	}

	glm::mat4 Transform::getMatrix() const
	{
		return glm::translate(glm::mat4(1.0), translation) *
			glm::mat4_cast(rotation) *
			glm::scale(glm::mat4(1.0), scale);
	}

	glm::mat4 Transform::getWorldMatrix()
	{
		if (!is_world_matrix_update)
		{
			world_matrix;
		}

		world_matrix = getMatrix();

		auto parent = node->getParent();

		if (parent && parent->hasComponent<Transform>())
		{
			auto& transform = parent->getComponent<Transform>();
			world_matrix = world_matrix * transform.getWorldMatrix();
		}

		is_world_matrix_update = false;

		return world_matrix;
	}
}