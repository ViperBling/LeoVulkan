#include "VKTexture.hpp"

namespace LeoVK
{

    void Texture::Destroy()
    {

    }

    void Texture::UpdateDescriptor()
    {

    }

    ktxResult Texture::LoadKTXFile(std::string filename, ktxTexture **target)
    {
        return KTX_FILE_SEEK_ERROR;
    }

    void Texture::LoadFromImage(
        tinygltf::Image& gltfImage,
        TextureSampler textureSampler,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue)
    {

    }

    void Texture2D::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {

    }

    void Texture2D::LoadFromBuffer(
        void *buffer,
        VkDeviceSize bufferSize,
        VkFormat format,
        uint32_t width, uint32_t height,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkFilter filter,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {

    }

    void Texture2DArray::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {

    }

    void TextureCube::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {

    }


}