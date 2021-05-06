#include <scene/components/texture.h>
#include <scene/components/astc.h>

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
		vkDestroyImageView(device->logicalDevice, view, nullptr);
		vkDestroyImage(device->logicalDevice, image, nullptr);
		if (sampler)
		{
			vkDestroySampler(device->logicalDevice, sampler, nullptr);
		}
		vkFreeMemory(device->logicalDevice, device_memory, nullptr);
	}

	std::unique_ptr<Image> Texture::load(const std::string& filename, vks::VulkanDevice* device)
	{
		auto image = Image::load(filename);
		if (image->isAstc())
		{
			if (!image->checkFormatSupport(*device))
			{
				image = std::make_unique<Astc>(*image);
				image->generateMipmap();
			}
		}

		return image;
	}

	void Texture2D::loadFromFile(const std::string& filename, vks::VulkanDevice* device, VkQueue copy_queue, VkImageUsageFlags image_usage_flags, VkImageLayout image_layout, bool force_linear)
	{
		auto img = Texture::load(filename, device);

		this->device = device;

		width = img->getExtent().width;
		height = img->getExtent().height;
		depth = img->getExtent().depth;
		mip_level = static_cast<uint32_t>(img->getMipMaps().size());

		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(device->physicalDevice, img->getFormat(), &formatProperties);

		VkBool32 useStaging = !force_linear;

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		if (useStaging)
		{
			// Create a host-visible staging buffer that contains the raw image data
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;

			VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
			bufferCreateInfo.size = img->getData().size();
			// This buffer is used as a transfer source for the buffer copy
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

			// Get memory requirements for the staging buffer (alignment, memory type bits)
			vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

			memAllocInfo.allocationSize = memReqs.size;
			// Get memory type index for a host visible buffer
			memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

			// Copy texture data into staging buffer
			uint8_t* data;
			VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
			memcpy(data, img->getData().data(), img->getData().size());
			vkUnmapMemory(device->logicalDevice, stagingMemory);

			// Setup buffer copy regions for each mip level
			std::vector<VkBufferImageCopy> bufferCopyRegions;

			for (auto& mipmap: img->getMipMaps())
			{
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = mipmap.level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent = mipmap.extent;
				bufferCopyRegion.bufferOffset = mipmap.offset;

				bufferCopyRegions.push_back(bufferCopyRegion);
			}

			// Create optimal tiled target image
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = img->getFormat();
			imageCreateInfo.mipLevels = mip_level;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.extent = { width, height, 1 };
			imageCreateInfo.usage = image_usage_flags;
			// Ensure that the TRANSFER_DST bit is set for staging
			if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
			{
				imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			}
			VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

			vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

			memAllocInfo.allocationSize = memReqs.size;

			memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &device_memory));
			VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, device_memory, 0));

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = mip_level;
			subresourceRange.layerCount = 1;

			// Image barrier for optimal image (target)
			// Optimal image will be used as destination for the copy
			vks::tools::setImageLayout(
				copyCmd,
				image,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				subresourceRange);

			// Copy mip levels from staging buffer
			vkCmdCopyBufferToImage(
				copyCmd,
				stagingBuffer,
				image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()),
				bufferCopyRegions.data()
			);

			// Change texture image layout to shader read after all mip levels have been copied
			this->image_layout = image_layout;
			vks::tools::setImageLayout(
				copyCmd,
				image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				image_layout,
				subresourceRange);

			device->flushCommandBuffer(copyCmd, copy_queue);

			// Clean up staging resources
			vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
			vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
		}
		else
		{
			// Prefer using optimal tiling, as linear tiling 
			// may support only a small set of features 
			// depending on implementation (e.g. no mip maps, only one layer, etc.)

			// Check if this support is supported for linear tiling
			assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

			VkImage mappableImage;
			VkDeviceMemory mappableMemory;

			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = img->getFormat();
			imageCreateInfo.extent = { width, height, 1 };
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
			imageCreateInfo.usage = image_usage_flags;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			// Load mip map level 0 to linear tiling image
			VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &mappableImage));

			// Get memory requirements for this image 
			// like size and alignment
			vkGetImageMemoryRequirements(device->logicalDevice, mappableImage, &memReqs);
			// Set memory allocation size to required memory size
			memAllocInfo.allocationSize = memReqs.size;

			// Get memory type that can be mapped to host memory
			memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			// Allocate host memory
			VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &mappableMemory));

			// Bind allocated image for use
			VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, mappableImage, mappableMemory, 0));

			// Get sub resource layout
			// Mip map count, array layer, etc.
			VkImageSubresource subRes = {};
			subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subRes.mipLevel = 0;

			VkSubresourceLayout subResLayout;
			void* data;

			// Get sub resources layout 
			// Includes row pitch, size offsets, etc.
			vkGetImageSubresourceLayout(device->logicalDevice, mappableImage, &subRes, &subResLayout);

			// Map image memory
			VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, mappableMemory, 0, memReqs.size, 0, &data));

			// Copy image data into memory
			memcpy(data, img->getData().data(), memReqs.size);

			vkUnmapMemory(device->logicalDevice, mappableMemory);

			// Linear tiled images don't need to be staged
			// and can be directly used as textures
			image = mappableImage;
			device_memory = mappableMemory;
			this->image_layout = image_layout;

			// Setup image memory barrier
			vks::tools::setImageLayout(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, image_layout);

			device->flushCommandBuffer(copyCmd, copy_queue);
		}

		// Create a default sampler
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		// Max level-of-detail should match mip level count
		samplerCreateInfo.maxLod = (useStaging) ? (float)img->getMipMaps().size() : 0.0f;
		// Only enable anisotropic filtering if enabled on the device
		samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
		samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

		// Create image view
		// Textures are not directly accessed by the shaders and
		// are abstracted by image views containing additional
		// information and sub resource ranges
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = img->getFormat();
		viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		// Linear tiling usually won't support mip maps
		// Only set mip map count if optimal tiling is used
		viewCreateInfo.subresourceRange.levelCount = (useStaging) ? mip_level : 1;
		viewCreateInfo.image = image;
		VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

	void Texture2D::loadFromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t texWidth, uint32_t texHeight, vks::VulkanDevice* device, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
	{
		assert(buffer);

		this->device = device;
		width = texWidth;
		height = texHeight;
		mip_level = 1;

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Use a separate command buffer for texture loading
		VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = bufferSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t* data;
		VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
		memcpy(data, buffer, bufferSize);
		vkUnmapMemory(device->logicalDevice, stagingMemory);

		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = width;
		bufferCopyRegion.imageExtent.height = height;
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = 0;

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = mip_level;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { width, height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		{
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

		vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;

		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &device_memory));
		VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, device_memory, 0));

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mip_level;
		subresourceRange.layerCount = 1;

		// Image barrier for optimal image (target)
		// Optimal image will be used as destination for the copy
		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy mip levels from staging buffer
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion
		);

		// Change texture image layout to shader read after all mip levels have been copied
		this->image_layout = image_layout;
		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			image_layout,
			subresourceRange);

		device->flushCommandBuffer(copyCmd, copyQueue);

		// Clean up staging resources
		vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
		vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

		// Create sampler
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = filter;
		samplerCreateInfo.minFilter = filter;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

		// Create image view
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = NULL;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = format;
		viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.image = image;
		VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

}
