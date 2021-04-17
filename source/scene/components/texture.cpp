#include <scene/components/texture.h>

#include	<filesystem>

namespace chaf
{
	void Texture::updateDescriptor()
	{
		descriptor.sampler = sampler;
		descriptor.imageView = view;
		descriptor.imageLayout = image_layout;
	}

	void Texture::destory()
	{
		vkDestroyImageView(device.logicalDevice, view, nullptr);
		vkDestroyImage(device.logicalDevice, image, nullptr);
		if (sampler)
		{
			vkDestroySampler(device.logicalDevice, sampler, nullptr);
		}
		vkFreeMemory(device.logicalDevice, device_memory, nullptr);
	}

	ktxResult Texture::loadKTX(const std::string& filename, ktxTexture** target)
	{
		return ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);
	}

	bool Texture::loadStb(const std::string& filename, stbi_uc** target)
	{
		int _width = 0;
		int _height = 0;
		int _channels = 0;
		stbi_uc* data = stbi_load(filename.c_str(), &_width, &_height, &_channels, 0);
		
		return data ? true : false;
	}

}
