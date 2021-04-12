#include <scene/components/camera.h>
#include <scene/components/transform.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#undef far
#undef near

namespace chaf
{
	Camera::Camera(Node* node, CameraType type) :
		type{ type },
		node{ node }
	{
	}

	void Camera::setType(CameraType type)
	{
		this->type = type;
	}

	CameraType Camera::getType() const
	{
		return type;
	}

	void Camera::setNearPlane(float near)
	{
		this->near_plane = near;
	}

	void Camera::setFarPlane(float far)
	{
		this->far_plane = far;
	}

	float Camera::getNearPlane() const
	{
		return near_plane;
	}

	float Camera::getFarPlane() const
	{
		return far_plane;
	}

	void Camera::setLeft(float left)
	{
		this->left = left;
	}

	void Camera::setRight(float right)
	{
		this->right = right;
	}

	void Camera::setTop(float top)
	{
		this->top = top;
	}

	void Camera::setBottom(float bottom)
	{
		this->bottom = bottom;
	}

	float Camera::getLeft() const
	{
		return left;
	}

	float Camera::getRight() const
	{
		return right;
	}

	float Camera::getTop() const
	{
		return top;
	}

	float Camera::getBottom() const
	{
		return bottom;
	}

	void Camera::setAspectRatio(float aspect_ratio)
	{
		this->aspect_ratio = aspect_ratio;
	}

	void Camera::setFieldOfView(float fov)
	{
		this->fov = fov;
	}

	float Camera::getAspectRatio() const
	{
		return aspect_ratio;
	}

	float Camera::getFieldOfView() const
	{
		return fov;
	}

	glm::mat4 Camera::getProjection() const
	{
		switch (type)
		{
		case chaf::CameraType::Perspective:
			return glm::perspective(fov, aspect_ratio, far_plane, near_plane);
			break;
		case chaf::CameraType::Orthographic:
			return glm::ortho(left, right, bottom, top, far_plane, near_plane);
			break;
		default:
			assert("unknown camera type!");
			return glm::mat4(1.f);
			break;
		}
	}

	glm::mat4 Camera::getView() const
	{
		auto& transform = node->getComponent<Transform>();
		return glm::inverse(transform.getWorldMatrix());
	}
}