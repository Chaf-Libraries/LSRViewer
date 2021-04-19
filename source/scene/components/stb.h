#pragma once

#include <scene/components/image.h>

namespace chaf
{
	class Stb: public Image
	{
	public:
		Stb(const std::string& filename, const std::vector<uint8_t>& data);

		virtual ~Stb() = default;	
	};
}