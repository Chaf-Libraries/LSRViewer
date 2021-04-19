#pragma once

#include <scene/components/image.h>

namespace chaf
{
	class Astc :public Image
	{
	public:
		struct BlockDim
		{
			uint8_t x;
			uint8_t y;
			uint8_t z;
		};

	public:
		Astc(const Image& image);

		Astc(const std::string& filename, const std::vector<uint8_t>& data);

		virtual ~Astc() = default;

	private:
		void decode(BlockDim block_dim, VkExtent3D extent, const uint8_t* data);

		void init();
	};
}