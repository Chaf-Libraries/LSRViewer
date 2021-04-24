#include <scene/components/image.h>
#include <scene/components/ktx.h>
#include <scene/components/stb.h>
#include <scene/components/astc.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

#include <filesystem>

namespace chaf
{
	Image::Image(const std::string& filename, std::vector<uint8_t>&& data, std::vector<MipMap>&& mipmaps) :
		filename{ filename },
		data{ std::move(data) },
		mipmaps{ std::move(mipmaps) }
	{
	}

	std::unique_ptr<Image> Image::load(const std::string& filename)
	{
		std::unique_ptr<Image> image{ nullptr };

		std::vector<uint8_t> raw_data;

		std::ifstream file;

		file.open(filename, std::ios::in | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file: " + filename);
		}

		file.seekg(0, std::ios::end);
		uint64_t read_count = static_cast<uint64_t>(file.tellg());
		file.seekg(0, std::ios::beg);

		raw_data.resize(static_cast<size_t>(read_count));
		file.read(reinterpret_cast<char*>(raw_data.data()), read_count);
		file.close();

		auto ext = std::filesystem::path(filename).extension();

		if (ext == ".ktx")
		{
			image = std::make_unique<Ktx>(filename, raw_data);
		}
		else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
		{
			image = std::make_unique<Stb>(filename, raw_data);
		}
		else if (ext == ".astc")
		{
			image = std::make_unique<Astc>(filename, raw_data);
		}

		return image;
	}

	void Image::generateMipmap()
	{
		if (mipmaps.size() > 1)
		{
			return;        // Do not generate again
		}

		auto extent = mipmaps.at(0).extent;
		auto next_width = std::max<uint32_t>(1u, extent.width / 2);
		auto next_height = std::max<uint32_t>(1u, extent.height / 2);
		auto channels = 4;
		auto next_size = next_width * next_height * channels;

		while (true)
		{
			// Make space for next mipmap
			auto old_size = static_cast<uint32_t>(data.size());
			data.resize(old_size + next_size);

			auto& prev_mipmap = mipmaps.back();
			// Update mipmaps
			MipMap next_mipmap{};
			next_mipmap.level = prev_mipmap.level + 1;
			next_mipmap.offset = old_size;
			next_mipmap.extent = { next_width, next_height, 1u };

			// Fill next mipmap memory
			stbir_resize_uint8(data.data() + prev_mipmap.offset, prev_mipmap.extent.width, prev_mipmap.extent.height, 0,
				data.data() + next_mipmap.offset, next_mipmap.extent.width, next_mipmap.extent.height, 0, channels);

			mipmaps.emplace_back(std::move(next_mipmap));

			// Next mipmap values
			next_width = std::max<uint32_t>(1u, next_width / 2);
			next_height = std::max<uint32_t>(1u, next_height / 2);
			next_size = next_width * next_height * channels;

			if (next_width == 1 && next_height == 1)
			{
				break;
			}
		}
	}

	bool Image::checkFormatSupport(vks::VulkanDevice& device)
	{
		VkImageFormatProperties format_properties;

		auto result = vkGetPhysicalDeviceImageFormatProperties(device.physicalDevice,
			format,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT,
			0,        // no create flags
			&format_properties);
		return result != VK_ERROR_FORMAT_NOT_SUPPORTED;
	}

	bool Image::isAstc()
	{
		return (format == VK_FORMAT_ASTC_4x4_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_4x4_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_5x4_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_5x4_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_5x5_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_5x5_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_6x5_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_6x5_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_6x6_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_6x6_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_8x5_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_8x5_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_8x6_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_8x6_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_8x8_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_8x8_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_10x5_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_10x5_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_10x6_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_10x6_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_10x8_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_10x8_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_10x10_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_10x10_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_12x10_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_12x10_SRGB_BLOCK ||
			format == VK_FORMAT_ASTC_12x12_UNORM_BLOCK ||
			format == VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
	}

	const std::string Image::getName() const
	{
		return filename;
	}

	VkFormat Image::getFormat() const
	{
		return format;
	}

	const VkExtent3D& Image::getExtent() const
	{
		return mipmaps.at(0).extent;
	}

	const std::vector<uint8_t>& Image::getData() const
	{
		return data;
	}

	const std::vector<Image::MipMap>& Image::getMipMaps() const
	{
		return mipmaps;
	}

	uint32_t Image::getLayers() const
	{
		return layers;
	}

	std::vector<uint8_t>& Image::getMutData()
	{
		return data;
	}

	void Image::setData(const uint8_t* raw_data, size_t size)
	{
		assert(data.empty() && "Image data already set");
		data = { raw_data, raw_data + size };
	}

	void Image::setFormat(VkFormat format)
	{
		this->format = format;
	}

	void Image::setWidth(uint32_t width)
	{
		mipmaps.at(0).extent.width = width;
	}

	void Image::setHeight(uint32_t height)
	{
		mipmaps.at(0).extent.height = height;
	}

	void Image::setDepth(uint32_t depth)
	{
		mipmaps.at(0).extent.depth = depth;
	}

	void Image::setLayers(uint32_t layers)
	{
		this->layers = layers;
	}

	void Image::setOffsets(const std::vector<std::vector<VkDeviceSize>>& offsets)
	{
		this->offsets = offsets;
	}

	std::vector<Image::MipMap>& Image::getMutMipmaps()
	{
		return mipmaps;
	}
}