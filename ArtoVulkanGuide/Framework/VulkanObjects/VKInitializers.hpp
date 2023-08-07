#pragma once

#include "VKTypes.hpp"

namespace VKInit
{
    VkCommandPoolCreateInfo CmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
    VkCommandBufferAllocateInfo CmdBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VkCommandBufferBeginInfo CmdBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
    VkCommandBufferSubmitInfo CmdBufferSubmitInfo(VkCommandBuffer cmdBuffer);

    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0);
    VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

    VkSubmitInfo2 SubmitInfo2(VkCommandBufferSubmitInfo* cmdSubmit, VkSemaphoreSubmitInfo* signal, VkSemaphoreSubmitInfo* wait);
    VkPresentInfoKHR PresentInfo();

    VkRenderingAttachmentInfo ColorAttachmentInfo(VkImageView view, VkClearValue clearValue, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo RenderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment);

    VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask);
    VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
}