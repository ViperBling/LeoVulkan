#include "VKTypes.hpp"
#include "VKEngine.hpp"

namespace VKUtil
{
    void TransitionImage(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void CopyImageToImage(VkCommandBuffer cmdBuffer, VkImage source, VkImage dest, VkExtent3D imageExtent);
}