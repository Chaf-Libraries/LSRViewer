#pragma once

#include <scene/components/image.h>

namespace chaf
{
	class Ktx :public Image
	{
	public:
		Ktx(const std::string& filename, std::vector<uint8_t>& data);

		virtual ~Ktx() = default;
	};
}