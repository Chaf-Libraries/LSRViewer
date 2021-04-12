#pragma once

#include <scene/components/primitive.h>

namespace chaf
{
	class Mesh
	{
	public:
		Mesh() = default;

		~Mesh() = default;

		void addPrimitive(Primitive& primitive);

		const std::vector<Primitive>& getPrimitives() const;

		AABB& getBounds();

	private:
		std::vector<Primitive> primitives;

		AABB bbox;
	};
}