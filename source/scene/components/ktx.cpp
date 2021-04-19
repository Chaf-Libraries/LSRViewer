#include <scene/components/ktx.h>

#include <ktx.h>
#include <vk_format.h>

namespace chaf
{
	static KTX_error_code KTXAPIENTRY optimal_tiling_callback(int mip_level,
		int /*face*/,
		int          width,
		int          height,
		int          depth,
		ktx_uint32_t face_lod_size,
		void* /*pixels*/,
		void* user_data)
	{
		// Get mipmaps
		auto& mipmaps = *reinterpret_cast<std::vector<Image::MipMap> *>(user_data);
		assert(static_cast<size_t>(mip_level) < mipmaps.size() && "Not enough space in the mipmap vector");

		auto& mipmap = mipmaps.at(mip_level);
		mipmap.level = mip_level;
		mipmap.extent.width = width;
		mipmap.extent.height = height;
		mipmap.extent.depth = depth;

		// Set offset for the next mip level
		auto next_mip_level = static_cast<size_t>(mip_level + 1);
		if (next_mip_level < mipmaps.size())
		{
			mipmaps.at(next_mip_level).offset = mipmap.offset + face_lod_size;
		}

		return KTX_SUCCESS;
	}



	Ktx::Ktx(const std::string& filename, std::vector<uint8_t>& data) :
		Image{ filename }
	{
		auto data_buffer = reinterpret_cast<const ktx_uint8_t *>(data.data());
	auto data_size   = static_cast<ktx_size_t>(data.size());

	ktxTexture *texture;
	auto        load_ktx_result = ktxTexture_CreateFromMemory(data_buffer,
                                                       data_size,
                                                       KTX_TEXTURE_CREATE_NO_FLAGS,
                                                       &texture);
	if (load_ktx_result != KTX_SUCCESS)
	{
		throw std::runtime_error{"Error loading KTX texture: " + filename};
	}

	if (texture->pData)
	{
		// Already loaded
		setData(texture->pData, texture->dataSize);
	}
	else
	{
		// Load
		auto &mut_data = getMutData();
		auto  size     = ktxTexture_GetSize(texture);
		mut_data.resize(size);
		auto load_data_result = ktxTexture_LoadImageData(texture, mut_data.data(), size);
		if (load_data_result != KTX_SUCCESS)
		{
			throw std::runtime_error{"Error loading KTX image data: " + filename};
		}
	}

	// Update width and height
	setWidth(texture->baseWidth);
	setHeight(texture->baseHeight);
	setDepth(texture->baseDepth);
	setLayers(texture->numLayers);

	bool cubemap = false;

	// Use the faces if there are 6 (for cubemap)
	if (texture->numLayers == 1 && texture->numFaces == 6)
	{
		cubemap = true;
		setLayers(texture->numFaces);
	}

	// Update format
	auto updated_format = vkGetFormatFromOpenGLInternalFormat(texture->glInternalformat);
	setFormat(updated_format);

	// Update mip levels
	auto &mipmap_levels = getMutMipmaps();
	mipmap_levels.resize(texture->numLevels);
	auto result = ktxTexture_IterateLevels(texture, optimal_tiling_callback, &mipmap_levels);
	if (result != KTX_SUCCESS)
	{
		throw std::runtime_error("Error loading KTX texture");
	}

	// If the texture contains more than one layer, then populate the offsets otherwise take the mipmap level offsets
	if (texture->numLayers > 1 || cubemap)
	{
		uint32_t layer_count = cubemap ? texture->numFaces : texture->numLayers;

		std::vector<std::vector<VkDeviceSize>> offsets;
		for (uint32_t layer = 0; layer < layer_count; layer++)
		{
			std::vector<VkDeviceSize> layer_offsets{};
			for (uint32_t level = 0; level < texture->numLevels; level++)
			{
				ktx_size_t     offset;
				KTX_error_code result;
				if (cubemap)
				{
					result = ktxTexture_GetImageOffset(texture, level, 0, layer, &offset);
				}
				else
				{
					result = ktxTexture_GetImageOffset(texture, level, layer, 0, &offset);
				}
				layer_offsets.push_back(static_cast<VkDeviceSize>(offset));
			}
			offsets.push_back(layer_offsets);
		}
		setOffsets(offsets);
	}
	else
	{
		std::vector<std::vector<VkDeviceSize>> offsets{};
		offsets.resize(1);
		for (size_t level = 0; level < mipmap_levels.size(); level++)
		{
			offsets[0].push_back(static_cast<VkDeviceSize>(mipmap_levels[level].offset));
		}
		setOffsets(offsets);
	}

	ktxTexture_Destroy(texture);
	}

}