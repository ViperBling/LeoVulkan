#pragma once

#include "ProjectPCH.hpp"

#include "VKBuffer.hpp"
#include "VKDevice.hpp"
#include "VKTools.hpp"

#include "tiny_gltf.h"

namespace LeoVK
{
    struct TextureSampler
    {
        VkFilter mMagFilter;
        VkFilter mMinFilter;
        VkSamplerAddressMode mAddressModeU;
        VkSamplerAddressMode mAddressModeV;
        VkSamplerAddressMode mAddressModeW;
    };

    class Texture
    {
    public:
        void Destroy();
        void UpdateDescriptor();
        ktxResult LoadKTXFile(std::string filename, ktxTexture** target);
        virtual void loadFromBuffer(
            void* buffer,
            VkDeviceSize bufferSize,
            VkFormat format,
            uint32_t width,
            uint32_t height,
            LeoVK::VulkanDevice *device,
            VkQueue copyQueue,
            TextureSampler texSampler,
            VkFilter filter = VK_FILTER_LINEAR,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

    public:
        LeoVK::VulkanDevice*  mpDevice;
        VkImage               mImage = VK_NULL_HANDLE;
        VkImageLayout         mImageLayout;
        VkDeviceMemory        mDeviceMemory;
        VkImageView           mView;
        uint32_t              mWidth, mHeight;
        uint32_t              mMipLevels;
        uint32_t              mLayerCount;
        VkDescriptorImageInfo mDescriptor;
        VkSampler             mSampler;
    };

    class Texture2D : public Texture
    {
    public:
        void LoadFromImage(
            tinygltf::Image& gltfImage,
            TextureSampler textureSampler,
            LeoVK::VulkanDevice* device,
            VkQueue copyQueue
        );

        void LoadFromFile(
            std::string filename,
            VkFormat format,
            LeoVK::VulkanDevice *device,
            VkQueue copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

    };

    class Texture2DArray : public Texture
    {
    public:
        void LoadFromFile(
            std::string filename,
            VkFormat format,
            LeoVK::VulkanDevice *device,
            VkQueue copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    };

    class TextureCube : public Texture
    {
    public:
        void LoadFromFile(
            std::string filename,
            VkFormat format,
            LeoVK::VulkanDevice *device,
            VkQueue copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    };

}