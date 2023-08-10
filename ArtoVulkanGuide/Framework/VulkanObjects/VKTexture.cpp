#include <iostream>

#include "VKTexture.hpp"
#include "VKInitializers.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace VKUtil
{
    bool LoadImageFromFile(VulkanEngine& engine, const std::string& filename, AllocatedImage& outImage)
    {
        int texW, texH, texC;
        stbi_uc* pixels = stbi_load(filename.c_str(), &texW, &texH, &texC, STBI_rgb_alpha);

        if (!pixels)
        {
            std::cout << "Fail to load texture: " << filename << std::endl;
            return false;
        }

        void* pixelPtr = pixels;
        VkDeviceSize imageSize = texW * texH * 4;
        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
        AllocatedBuffer stagingBuffer = engine.CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_COPY);

        void* data;
        vmaMapMemory(engine.mAllocator, stagingBuffer.mAllocation, &data);
        memcpy(data, pixelPtr, static_cast<size_t>(imageSize));
        vmaUnmapMemory(engine.mAllocator, stagingBuffer.mAllocation);

        stbi_image_free(pixels);

        VkExtent3D imageExtent;
        imageExtent.width = static_cast<uint32_t>(texW);
        imageExtent.height = static_cast<uint32_t>(texH);
        imageExtent.depth = 1;

        VkImageCreateInfo imageCI = VKInit::ImageCreateInfo(imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);
        AllocatedImage newImage{};
        VmaAllocationCreateInfo vmaAllocCI {};
        vmaAllocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(engine.mAllocator, &imageCI, &vmaAllocCI, &newImage.mImage, &newImage.mAllocation, nullptr);

        engine.ImmediateSubmit([&](VkCommandBuffer cmdBuffer)
        {
            VkImageSubresourceRange imageSubRange;
            imageSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageSubRange.baseMipLevel = 0;
            imageSubRange.levelCount = 1;
            imageSubRange.baseArrayLayer = 0;
            imageSubRange.layerCount = 1;

            VkImageMemoryBarrier imageBarrierToTransfer {};
            imageBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrierToTransfer.image = newImage.mImage;
            imageBarrierToTransfer.subresourceRange = imageSubRange;
            imageBarrierToTransfer.srcAccessMask = 0;
            imageBarrierToTransfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(
                cmdBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0,
                nullptr, 0,
                nullptr, 1,
                &imageBarrierToTransfer);

            VkBufferImageCopy copyRegion {};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = imageExtent;

            //copy the buffer into the image
            vkCmdCopyBufferToImage(
                cmdBuffer,
                stagingBuffer.mBuffer,
                newImage.mImage,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &copyRegion);

            VkImageMemoryBarrier imageBarrierToReadable = imageBarrierToTransfer;
            imageBarrierToReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrierToReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageBarrierToReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrierToReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0,
                nullptr, 0,
                nullptr, 1,
                &imageBarrierToReadable);
        });

        engine.mMainDeletionQueue.PushFunction([=]()
        {
            vmaDestroyImage(engine.mAllocator, newImage.mImage, newImage.mAllocation);
        });
        vmaDestroyBuffer(engine.mAllocator, stagingBuffer.mBuffer, stagingBuffer.mAllocation);

        std::cout << "Texture load successfully" << filename << std::endl;
        outImage = newImage;

        return true;
    }
}