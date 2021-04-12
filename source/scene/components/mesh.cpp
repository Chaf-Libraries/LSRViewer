#include <scene/components/mesh.h>

namespace chaf
{
	void Mesh::addPrimitive(Primitive& primitive)
	{
		primitives.push_back(primitive);
		bbox.update(primitive.bbox.getMin());
		bbox.update(primitive.bbox.getMax());
	}

	const std::vector<Primitive>& Mesh::getPrimitives() const
	{
		return primitives;
	}

	AABB& Mesh::getBounds()
	{
		return bbox;
	}
}