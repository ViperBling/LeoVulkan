#pragma once

#include <string>

#include "VKTypes.hpp"

namespace VKInit
{
    VkCommandPoolCreateInfo CmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
    VkCommandBufferAllocateInfo CmdBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    VkCommandBufferBeginInfo CmdBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
    VkCommandBufferSubmitInfo CmdBufferSubmitInfo(VkCommandBuffer cmdBuffer);

    VkFramebufferCreateInfo FrameBufferCreateInfo(VkRenderPass renderPass, VkExtent2D extent);

    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0);
    VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

    VkSubmitInfo SubmitInfo(VkCommandBuffer* cmdBuffer);
    VkSubmitInfo2 SubmitInfo2(VkCommandBufferSubmitInfo* cmdSubmit, VkSemaphoreSubmitInfo* signal, VkSemaphoreSubmitInfo* wait);
    VkPresentInfoKHR PresentInfo();

    VkRenderPassBeginInfo RenderPassBeginInfo(VkRenderPass renderPass, VkExtent2D extent, VkFramebuffer frameBuffer);
    VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char* entry = "main");

    VkPipelineVertexInputStateCreateInfo PipelineVIStateCreateInfo();
    VkPipelineInputAssemblyStateCreateInfo PipelineIAStateCreateInfo(VkPrimitiveTopology topology);
    VkPipelineRasterizationStateCreateInfo PipelineRSStateCreateInfo(VkPolygonMode polygonMode);
    VkPipelineMultisampleStateCreateInfo PipelineMSStateCreateInfo();
    VkPipelineColorBlendAttachmentState PipelineCBAttachState();

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo();

    VkRenderingAttachmentInfo ColorAttachmentInfo(VkImageView view, VkClearValue clearValue, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo RenderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment);

    VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask);
    VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

    VkDescriptorSetLayoutBinding DescSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
    VkWriteDescriptorSet WriteDescImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);
    VkImageCreateInfo ImageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
    VkImageViewCreateInfo ImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
}