#pragma once

#include <iostream>
#include <deque>
#include <functional>
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

struct DeletionQueue
{
    std::deque<std::function<void()>> mDeletors;

    void PushFunction(std::function<void()>&& function)
    {
        mDeletors.push_back(function);
    }

    void Flush()
    {
        for (auto it = mDeletors.rbegin(); it != mDeletors.rend(); it++)
        {
            (*it)();
        }
        mDeletors.clear();
    }
};

struct AllocatedBuffer
{
    VkBuffer mBuffer;
    VmaAllocation mAllocation;
};

struct AllocatedImage
{
    VkImage mImage;
    VmaAllocation mAllocation;
};