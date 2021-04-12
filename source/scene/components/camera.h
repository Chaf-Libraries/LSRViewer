#pragma once

#include <glm/glm.hpp>

#include <scene/node.h>

namespace chaf
{
	enum class CameraType
	{
		Perspective,
		Orthographic
	};

	class Camera
	{
	public:
		Camera(Node* node, CameraType type = CameraType::Perspective);

		void setType(CameraType type);

		CameraType getType() const;

		void setNearPlane(float near);

		void setFarPlane(float far);

		float getNearPlane() const;

		float getFarPlane() const;

		// Orthographic camera setting
		void setLeft(float left);

		void setRight(float right);

		void setTop(float top);

		void setBottom(float bottom);

		float getLeft() const;

		float getRight() const;

		float getTop() const;

		float getBottom() const;

		// Perspective camera setting
		void setAspectRatio(float aspect_ratio);

		void setFieldOfView(float fov);

		float getAspectRatio() const;

		float getFieldOfView() const;

		glm::mat4 getProjection() const;

		glm::mat4 getView() const;

	private:
		CameraType type{ CameraType::Perspective };

		Node* node;

		float near_plane{ 0.01f };

		float far_plane{ 10000.f };

		// Orthographic camera parameter
		float left{ -1.0f };

		float right{ 1.0f };

		float bottom{ -1.0f };

		float top{ 1.0f };

		// Perspective camera parameter
		float aspect_ratio{ 1.0f };

		float fov{ glm::radians(60.0f) };
	};
}