#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#define VK_CHECK(x)                                                     \
	do                                                                  \
	{                                                                   \
		VkResult err = x;                                               \
		if (err)                                                        \
		{                                                               \
			std::cout <<"Detected Vulkan error: " << err << std::endl;  \
			abort();                                                    \
		}                                                               \
	} while (0)

struct AllocatedImage
{
    VkImage mImage;
    VmaAllocation mAllocation;
};