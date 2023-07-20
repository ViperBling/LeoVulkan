#include "VKFrameBuffer.hpp"

namespace LeoVK
{
    bool FrameBufferAttachment::HasDepth()
    {
        std::vector<VkFormat> formats ={
            VK_FORMAT_D16_UNORM,
            VK_FORMAT_X8_D24_UNORM_PACK32,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        return std::find(formats.begin(), formats.end(), mFormat) != std::end(formats);
    }

    bool FrameBufferAttachment::HasStencil()
    {
        std::vector<VkFormat> formats ={
            VK_FORMAT_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        return std::find(formats.begin(), formats.end(), mFormat) != std::end(formats);
    }

    bool FrameBufferAttachment::IsDepthStencil()
    {
        return HasDepth() || HasStencil();
    }

    FrameBuffer::FrameBuffer(LeoVK::VulkanDevice *vulkanDevice)
    {
        assert(vulkanDevice);
        mpVulkanDevice = vulkanDevice;
    }

    FrameBuffer::~FrameBuffer()
    {
        assert(mpVulkanDevice);
        for (auto attachment : mAttachments)
        {
            vkDestroyImage(mpVulkanDevice->mLogicalDevice, attachment.mImage, nullptr);
            vkDestroyImageView(mpVulkanDevice->mLogicalDevice, attachment.mView, nullptr);
            vkFreeMemory(mpVulkanDevice->mLogicalDevice, attachment.mMemory, nullptr);
        }
        vkDestroySampler(mpVulkanDevice->mLogicalDevice, mSampler, nullptr);
        vkDestroyRenderPass(mpVulkanDevice->mLogicalDevice, mRenderPass, nullptr);
        vkDestroyFramebuffer(mpVulkanDevice->mLogicalDevice, mFrameBuffer, nullptr);
    }

    /**
	* Add a new attachment described by createinfo to the framebuffer's attachment list
	*
	* @param createinfo Structure that specifies the framebuffer to be constructed
	*
	* @return Index of the new attachment
	*/
    uint32_t FrameBuffer::AddAttachment(AttachmentCreateInfo createInfo)
    {
        LeoVK::FrameBufferAttachment attachment;
        attachment.format = createInfo.attachment;

        VkImageAspectFlags aspectMask = VK_FLAGS_NONE;

        // 选择一个aspect mask
        if (createInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        if (createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            if (attachment.HasDepth()) aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (attachment.HasStencil()) aspectMask = aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        assert(aspectMask > 0);

        VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = createInfo.format;
        imageCI.extent = {createInfo.width, createInfo.height, 1};
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = createInfo.layerCount;
        imageCI.samples = createInfo.imageSampleCount;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = createInfo.usage;

        VkMemoryAllocateInfo 
    }

    /**
	* Creates a default sampler for sampling from any of the framebuffer attachments
	* Applications are free to create their own samplers for different use cases
	*
	* @param magFilter Magnification filter for lookups
	* @param minFilter Minification filter for lookups
	* @param adressMode Addressing mode for the U,V and W coordinates
	*
	* @return VkResult for the sampler creation
	*/
    VkResult FrameBuffer::CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addresMode)
    {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    /**
	* Creates a default render pass setup with one sub pass
	*
	* @return VK_SUCCESS if all resources have been created successfully
	*/
    VkResult FrameBuffer::CreateRenderPass()
    {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
}