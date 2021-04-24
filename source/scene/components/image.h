#pragma once

#include <VulkanTools.h>
#include <VulkanDevice.h>

#include <vector>

namespace chaf
{
	class Image
	{
	public:
		struct MipMap
		{
			uint32_t level = 0;
			uint32_t offset = 0;
			VkExtent3D extent{ 0,0,0 };
		};

	public:
		Image(const std::string& filename, std::vector<uint8_t>&& data = {}, std::vector<MipMap>&& mipmaps = { {} });

		static std::unique_ptr<Image> load(const std::string& filename);

		virtual ~Image() = default;

		void generateMipmap();

		bool checkFormatSupport(vks::VulkanDevice& device);

		bool isAstc();

	public:
		const std::string getName() const;

		VkFormat getFormat() const;

		const VkExtent3D& getExtent() const;

		const std::vector<uint8_t>& getData() const;

		const std::vector<MipMap>& getMipMaps() const;

		uint32_t getLayers() const;

	protected:
		std::vector<uint8_t>& getMutData();

		void setData(const uint8_t* raw_data, size_t size);

		void setFormat(VkFormat format);

		void setWidth(uint32_t width);

		void setHeight(uint32_t height);

		void setDepth(uint32_t depth);

		void setLayers(uint32_t layers);

		void setOffsets(const std::vector<std::vector<VkDeviceSize>>& offsets);

		std::vector<MipMap>& getMutMipmaps();

	private:
		const std::string filename;

		std::vector<uint8_t> data;

		VkFormat format{ VK_FORMAT_UNDEFINED };

		uint32_t layers{ 1 };

		std::vector<MipMap> mipmaps{ {} };

		std::vector<std::vector<VkDeviceSize>> offsets;
	};
}