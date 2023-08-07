#include "VKInitializers.hpp"

namespace VKInit
{
    VkCommandPoolCreateInfo CmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
    {
        VkCommandPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.pNext = nullptr;

        info.flags = flags;
        return info;
    }

    VkCommandBufferAllocateInfo CmdBufferAllocateInfo(VkCommandPool pool, uint32_t count, VkCommandBufferLevel level)
    {
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.pNext = nullptr;

        info.commandPool = pool;
        info.commandBufferCount = count;
        info.level = level;
        return info;
    }

    VkCommandBufferBeginInfo CmdBufferBeginInfo(VkCommandBufferUsageFlags flags)
    {
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.pNext = nullptr;

        info.pInheritanceInfo = nullptr;
        info.flags = flags;
        return info;
    }

    VkCommandBufferSubmitInfo CmdBufferSubmitInfo(VkCommandBuffer cmdBuffer)
    {
        VkCommandBufferSubmitInfo cmdinfo{};
        cmdinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdinfo.pNext = nullptr;
        cmdinfo.commandBuffer = cmdBuffer;
        cmdinfo.deviceMask = 0;

        return cmdinfo;
    }

    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags)
    {
        VkFenceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        info.pNext = nullptr;

        info.flags = flags;

        return info;
    }

    VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags)
    {
        VkSemaphoreCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = flags;
        return info;
    }

    VkSubmitInfo2 SubmitInfo2(VkCommandBufferSubmitInfo *cmdSubmit, VkSemaphoreSubmitInfo *signal, VkSemaphoreSubmitInfo *wait)
    {
        VkSubmitInfo2 info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        info.pNext = nullptr;

        info.waitSemaphoreInfoCount = wait == nullptr? 0 : 1;
        info.pWaitSemaphoreInfos = wait;

        info.signalSemaphoreInfoCount = signal == nullptr ? 0 : 1;
        info.pSignalSemaphoreInfos = signal;

        info.commandBufferInfoCount = 1;
        info.pCommandBufferInfos = cmdSubmit;

        return info;
    }

    VkPresentInfoKHR PresentInfo()
    {
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.pNext = nullptr;

        info.swapchainCount = 0;
        info.pSwapchains = nullptr;
        info.pWaitSemaphores = nullptr;
        info.waitSemaphoreCount = 0;
        info.pImageIndices = nullptr;

        return info;
    }

    VkRenderingAttachmentInfo ColorAttachmentInfo(VkImageView view, VkClearValue clearValue, VkImageLayout layout)
    {
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.pNext = nullptr;

        colorAttachment.imageView = view;
        colorAttachment.imageLayout = layout;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearValue;

        return colorAttachment;
    }

    VkRenderingInfo RenderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo *colorAttachment)
    {
        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.pNext = nullptr;

        renderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0}, renderExtent };
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = colorAttachment;
        renderInfo.pDepthAttachment = nullptr;
        renderInfo.pStencilAttachment = nullptr;

        return renderInfo;
    }

    VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask)
    {
        VkImageSubresourceRange subImage{};
        subImage.aspectMask = aspectMask;
        subImage.baseMipLevel = 0;
        subImage.levelCount = 1;
        subImage.baseArrayLayer = 0;
        subImage.layerCount = 1;

        return subImage;
    }

    VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
    {
        VkSemaphoreSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.semaphore = semaphore;
        submitInfo.stageMask = stageMask;
        submitInfo.deviceIndex = 0;
        submitInfo.value = 1;

        return submitInfo;
    }
}