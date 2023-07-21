#include "VKTexture.hpp"

namespace LeoVK
{
    void ReadBufferFromImage(tinygltf::Image& image, unsigned char* buffer, VkDeviceSize& bufferSize, bool& deleteBuffer)
    {
        deleteBuffer = false;
        if (image.component == 3)
        {
            bufferSize = image.width * image.height * 4;
            buffer = new unsigned char[bufferSize];
            unsigned char* rgba = buffer;
            unsigned char* rgb = &image.image[0];
            for (size_t i = 0; i < image.width * image.height; ++i)
            {
                for (int32_t j = 0; j < 3; ++j) rgba[j] = rgb[j];
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }
        else
        {
            buffer = &image.image[0];
            bufferSize = image.image.size();
        }
    }


    void Texture::Destroy()
    {
        vkDestroyImageView(mpDevice->mLogicalDevice, mView, nullptr);
        vkDestroyImage(mpDevice->mLogicalDevice, mImage, nullptr);
        if (mSampler)
        {
            vkDestroySampler(mpDevice->mLogicalDevice, mSampler, nullptr);
        }
        vkFreeMemory(mpDevice->mLogicalDevice, mDeviceMemory, nullptr);
    }

    void Texture::UpdateDescriptor()
    {
        mDescriptor.sampler = mSampler;
        mDescriptor.imageView = mView;
        mDescriptor.imageLayout = mImageLayout;
    }

    ktxResult Texture::LoadKTXFile(std::string filename, ktxTexture **target)
    {
        ktxResult res = KTX_SUCCESS;
        if (!LeoVK::VKTools::FileExists(filename))
        {
            LeoVK::VKTools::ExitFatal(
                "Could not load texture from " + filename +
                "\n\nThe file may be part of the additional asset pack."
                "\\n\\nRun \\\"download_assets.py\\\" in the repository root to download the latest version.", -1);
        }
        res = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);
        return res;
    }

    void Texture::loadFromBuffer(
        void *buffer,
        VkDeviceSize bufferSize,
        VkFormat format,
        uint32_t width, uint32_t height,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        TextureSampler texSampler,
        VkFilter filter,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        assert(buffer);
        mWidth = width;
        mHeight = height;
        mMipLevels = static_cast<uint32_t>(floor(log2(std::max(width, height))) + 1.0);

        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(mpDevice->mPhysicalDevice, format, &formatProps);
        assert(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_BLIT_SRC_BIT);
        assert(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_BLIT_DST_BIT);

        VkMemoryAllocateInfo memAI = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        VkBuffer stageBuffer;
        VkDeviceMemory stageMem;

        VkBufferCreateInfo bufferCI = LeoVK::Init::BufferCreateInfo();
        bufferCI.size = bufferSize;
        bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCI, nullptr, &stageBuffer))
        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stageBuffer, &memReqs);
        memAI.allocationSize = memReqs.size;
        memAI.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &stageMem))
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stageBuffer, stageMem, 0))

        uint8_t* data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stageMem, 0, memReqs.size, 0, (void**)&data))
        memcpy(data, buffer, bufferSize);
        vkUnmapMemory(mpDevice->mLogicalDevice, stageMem);

        VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = format;
        imageCI.mipLevels = mMipLevels;
        imageCI.arrayLayers = 1;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCI.extent = { width, height, 1 };
        imageCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCI, nullptr, &mImage))
        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);
        memAI.allocationSize = memReqs.size;
        memAI.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &mDeviceMemory))
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0))

        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;
        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = width;
        bufferCopyRegion.imageExtent.height = height;
        bufferCopyRegion.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(copyCmd, stageBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        mpDevice->FlushCommandBuffer(copyCmd, copyQueue, true);
        vkFreeMemory(mpDevice->mLogicalDevice, stageMem, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stageBuffer, nullptr);

        // Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
        VkCommandBuffer blitCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        for (uint32_t i = 1; i < mMipLevels; i++)
        {
            VkImageBlit imageBlit{};

            imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.srcSubresource.layerCount = 1;
            imageBlit.srcSubresource.mipLevel = i - 1;
            imageBlit.srcOffsets[1].x = int32_t(width >> (i - 1));
            imageBlit.srcOffsets[1].y = int32_t(height >> (i - 1));
            imageBlit.srcOffsets[1].z = 1;

            imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.dstSubresource.layerCount = 1;
            imageBlit.dstSubresource.mipLevel = i;
            imageBlit.dstOffsets[1].x = int32_t(width >> i);
            imageBlit.dstOffsets[1].y = int32_t(height >> i);
            imageBlit.dstOffsets[1].z = 1;

            VkImageSubresourceRange mipSubRange = {};
            mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipSubRange.baseMipLevel = i;
            mipSubRange.levelCount = 1;
            mipSubRange.layerCount = 1;
            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = 0;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.image = mImage;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }
            vkCmdBlitImage(blitCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageMemoryBarrier.image = mImage;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }
        }

        subresourceRange.levelCount = mMipLevels;
        imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        mpDevice->FlushCommandBuffer(blitCmd, copyQueue, true);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = texSampler.mMagFilter;
        samplerInfo.minFilter = texSampler.mMinFilter;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = texSampler.mAddressModeU;
        samplerInfo.addressModeV = texSampler.mAddressModeV;
        samplerInfo.addressModeW = texSampler.mAddressModeW;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.maxAnisotropy = 1.0;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxLod = (float)mMipLevels;
        samplerInfo.maxAnisotropy = 8.0f;
        samplerInfo.anisotropyEnable = VK_TRUE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerInfo, nullptr, &mSampler));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = mImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.levelCount = mMipLevels;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewInfo, nullptr, &mView));

        UpdateDescriptor();
    }

    void Texture2D::LoadFromImage(
        tinygltf::Image& gltfImage,
        TextureSampler textureSampler,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue)
    {
        mpDevice = device;

        bool isKtx = false;
        // Image points to an external ktx file
        if (gltfImage.uri.find_last_of('.') != std::string::npos)
        {
            if (gltfImage.uri.substr(gltfImage.uri.find_last_of('.') + 1) == "ktx") isKtx = true;
        }

        TextureSampler sampler = {
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT
        };

        bool deleteBuffer = false;
        unsigned char* buffer = nullptr;
        VkDeviceSize bufferSize = 0;
        ReadBufferFromImage(gltfImage, buffer, bufferSize, deleteBuffer);

        loadFromBuffer(
            buffer, bufferSize,
            VK_FORMAT_R8G8B8A8_UNORM,
            gltfImage.width, gltfImage.height,
            mpDevice,
            copyQueue,
            sampler);

        if (deleteBuffer) delete[] buffer;
    }

    /**
	* Load a 2D texture including all mip levels
	*
	* @param filename File to load (supports .ktx)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	* @param (Optional) forceLinear Force linear tiling (not advised, defaults to false)
	*
	*/
    void Texture2D::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        gli::texture2d tex2D(gli::load(filename.c_str()));
        assert(!tex2D.empty());

        mpDevice = device;
        uint32_t width = static_cast<uint32_t>(tex2D[0].extent().x);
        uint32_t height = static_cast<uint32_t>(tex2D[0].extent().y);

        TextureSampler sampler = {
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT
        };

        loadFromBuffer(
            tex2D.data(),
            tex2D.size(),
            format, width, height,
            device,
            copyQueue,
            sampler,
            VK_FILTER_LINEAR,
            imageUsageFlags, imageLayout);
    }



    void Texture2DArray::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        gli::texture2d_array tex2D(gli::load(filename.c_str()));
        assert(!tex2D.empty());

        mpDevice = device;
        mWidth = static_cast<uint32_t>(tex2D[0].extent().x);
        mHeight = static_cast<uint32_t>(tex2D[0].extent().y);
        mLayerCount = static_cast<uint32_t>(tex2D.layers());
        mMipLevels = static_cast<uint32_t>(tex2D.levels());
        
        VkMemoryAllocateInfo memAllocInfo = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;
        
        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        VkBufferCreateInfo bufferCreateInfo = LeoVK::Init::BufferCreateInfo();
        bufferCreateInfo.size = tex2D.size();
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));
        
        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stagingBuffer, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stagingBuffer, stagingMemory, 0));
        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, tex2D.data(), tex2D.size());
        vkUnmapMemory(mpDevice->mLogicalDevice, stagingMemory);
        
        // Setup buffer copy regions for each layer including all of its miplevels
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        size_t offset = 0;
        for (uint32_t layer = 0; layer < mLayerCount; layer++)
        {
            for (uint32_t level = 0; level < mMipLevels; level++)
            {
                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex2D[layer][level].extent().x);
                bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex2D[layer][level].extent().y);
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;

                bufferCopyRegions.push_back(bufferCopyRegion);

                // Increase offset into staging buffer for next level / face
                offset += tex2D[layer][level].size();
            }
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = LeoVK::Init::ImageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { mWidth, mHeight, 1 };
        imageCreateInfo.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        imageCreateInfo.arrayLayers = mLayerCount;
        imageCreateInfo.mipLevels = mMipLevels;
        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCreateInfo, nullptr, &mImage));

        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &mDeviceMemory));
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0));

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Image barrier for optimal image (target)
        // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mMipLevels;
        subresourceRange.layerCount = mLayerCount;

        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy the layers and mip levels from the staging buffer to the optimal tiled image
        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

        // Change texture image layout to shader read after all faces have been copied
        mImageLayout = imageLayout;
        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

        mpDevice->FlushCommandBuffer(copyCmd, copyQueue);

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo = LeoVK::Init::SamplerCreateInfo();
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
        samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.maxAnisotropy = mpDevice->mEnabledFeatures.samplerAnisotropy ? mpDevice->mProperties.limits.maxSamplerAnisotropy : 1.0f;
        samplerCreateInfo.anisotropyEnable = mpDevice->mEnabledFeatures.samplerAnisotropy;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = (float)mMipLevels;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerCreateInfo, nullptr, &mSampler));

        // Create image view
        VkImageViewCreateInfo viewCreateInfo = LeoVK::Init::ImageViewCreateInfo();
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewCreateInfo.format = format;
        viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        viewCreateInfo.subresourceRange.layerCount = mLayerCount;
        viewCreateInfo.subresourceRange.levelCount = mMipLevels;
        viewCreateInfo.image = mImage;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewCreateInfo, nullptr, &mView));

        vkFreeMemory(mpDevice->mLogicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stagingBuffer, nullptr);

        // Update descriptor image info member that can be used for setting up descriptor sets
        UpdateDescriptor();
    }

    void TextureCube::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        gli::texture_cube texCube(gli::load(filename));
        assert(!texCube.empty());

        mpDevice = device;
        mWidth = static_cast<uint32_t>(texCube.extent().x);
        mHeight = static_cast<uint32_t>(texCube.extent().y);
        mMipLevels = static_cast<uint32_t>(texCube.levels());

        VkMemoryAllocateInfo memAllocInfo{};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements memReqs;

        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = texCube.size();
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stagingBuffer, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, texCube.data(), texCube.size());
        vkUnmapMemory(mpDevice->mLogicalDevice, stagingMemory);

        // Setup buffer copy regions for each face including all of it's miplevels
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        size_t offset = 0;

        for (uint32_t face = 0; face < 6; face++)
        {
            for (uint32_t level = 0; level < mMipLevels; level++)
            {
                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = face;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(texCube[face][level].extent().x);
                bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(texCube[face][level].extent().y);
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;

                bufferCopyRegions.push_back(bufferCopyRegion);

                // Increase offset into staging buffer for next level / face
                offset += texCube[face][level].size();
            }
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mMipLevels;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { mWidth, mHeight, 1 };
        imageCreateInfo.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        // Cube faces count as array layers in Vulkan
        imageCreateInfo.arrayLayers = 6;
        // This flag is required for cube map images
        imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCreateInfo, nullptr, &mImage));

        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &mDeviceMemory));
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0));

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Image barrier for optimal image (target)
        // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mMipLevels;
        subresourceRange.layerCount = 6;
        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }

        // Copy the cube map faces from the staging buffer to the optimal tiled image
        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

        // Change texture image layout to shader read after all faces have been copied
        mImageLayout = imageLayout;
        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout = imageLayout;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        mpDevice->FlushCommandBuffer(copyCmd, copyQueue);

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
        samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.maxAnisotropy = mpDevice->mEnabledFeatures.samplerAnisotropy ? mpDevice->mProperties.limits.maxSamplerAnisotropy : 1.0f;
        samplerCreateInfo.anisotropyEnable = mpDevice->mEnabledFeatures.samplerAnisotropy;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = (float)mMipLevels;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerCreateInfo, nullptr, &mSampler));

        // Create image view
        VkImageViewCreateInfo viewCreateInfo{};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewCreateInfo.format = format;
        viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        viewCreateInfo.subresourceRange.layerCount = 6;
        viewCreateInfo.subresourceRange.levelCount = mMipLevels;
        viewCreateInfo.image = mImage;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewCreateInfo, nullptr, &mView));

        // Clean up staging resources
        vkFreeMemory(mpDevice->mLogicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stagingBuffer, nullptr);

        // Update descriptor image info member that can be used for setting up descriptor sets
        UpdateDescriptor();
    }
}