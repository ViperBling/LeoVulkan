#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

struct AllocatedImage
{
    VkImage mImage;
    VkImageView mImageView;
    VmaAllocation mAllocation;
};

struct AllocatedBuffer
{
    VkBuffer mBuffer;
    VmaAllocation mAllocation;
};