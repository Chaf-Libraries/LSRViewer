#include <scene/components/stb.h>

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace chaf
{
	Stb::Stb(const std::string& filename, const std::vector<uint8_t>& data) :
		Image{ filename }
	{
		int width;
		int height;
		int comp;
		int req_comp = 4;

		auto data_buffer = reinterpret_cast<const stbi_uc*>(data.data());
		auto data_size = static_cast<int>(data.size());

		auto raw_data = stbi_load_from_memory(data_buffer, data_size, &width, &height, &comp, req_comp);

		if (!raw_data)
		{
			throw std::runtime_error{ "Failed to load " + filename + ": " + stbi_failure_reason() };
		}

		setData(raw_data, width * height * req_comp);
		stbi_image_free(raw_data);

		setFormat(VK_FORMAT_R8G8B8A8_UNORM);
		setWidth(static_cast<uint32_t>(width));
		setHeight(static_cast<uint32_t>(height));
		setDepth(1u);
	}
}