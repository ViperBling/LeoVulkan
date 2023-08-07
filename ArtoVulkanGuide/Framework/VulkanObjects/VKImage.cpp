#include "VKImage.hpp"

namespace VKUtil
{
    void TransitionImage(
        VkCommandBuffer cmdBuffer,
        VkImage image,
        VkImageLayout currentLayout,
        VkImageLayout newLayout)
    {
        VkImageMemoryBarrier2 imageBarrier {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.pNext = nullptr;

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        VkImageSubresourceRange subImage {};
        subImage.aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        subImage.baseMipLevel = 0;
        subImage.levelCount = 1;
        subImage.baseArrayLayer = 0;
        subImage.layerCount = 1;

        imageBarrier.subresourceRange = subImage;
        imageBarrier.image = image;

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;

        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }

    void CopyImageToImage(VkCommandBuffer cmdBuffer, VkImage source, VkImage dest, VkExtent3D imageExtent)
    {
        VkImageSubresourceRange subImage{};
        subImage.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subImage.baseMipLevel = 0;
        subImage.levelCount = 1;
        subImage.baseArrayLayer = 0;
        subImage.layerCount = 1;

        VkImageCopy2 copyRegion{};
        copyRegion.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
        copyRegion.pNext = nullptr;
        copyRegion.extent = imageExtent;

        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.baseArrayLayer = 0;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.srcSubresource.mipLevel = 0;

        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.baseArrayLayer = 0;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.dstSubresource.mipLevel = 0;

        VkCopyImageInfo2 copyInfo{};
        copyInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
        copyInfo.pNext = nullptr;
        copyInfo.dstImage = dest;
        copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyInfo.srcImage = source;
        copyInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        copyInfo.regionCount = 1;
        copyInfo.pRegions = &copyRegion;

        vkCmdCopyImage2(cmdBuffer, &copyInfo);
    }
}

