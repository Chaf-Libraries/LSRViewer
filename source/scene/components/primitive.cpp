#include <scene/components/primitive.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace chaf
{
	template <class T>
	inline void hash_combine(size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		glm::detail::hash_combine(seed, hasher(v));
	}

	void Primitive::updateID()
	{
		id = 0;

		hash_combine(id, first_index);
		hash_combine(id, index_count);
		hash_combine(id, material_index);
		hash_combine(id, this);
	}
}