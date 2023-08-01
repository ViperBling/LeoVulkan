﻿#include "VKDevice.hpp"

namespace LeoVK
{
    VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
    {
        assert(physicalDevice);
        mPhysicalDevice = physicalDevice;

        vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
        vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mFeatures);
        vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);

        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);
        assert(queueFamilyCount > 0);
        mQueueFamilyProperties.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, mQueueFamilyProperties.data());

        // Get list of supported extensions
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        if (extCount > 0)
        {
            std::vector<VkExtensionProperties> extensions(extCount);
            if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
            {
                for (auto ext : extensions)
                {
                    mSupportedExtensions.emplace_back(ext.extensionName);
                }
            }
        }
    }

    VulkanDevice::~VulkanDevice()
    {
        if (mCommandPool) vkDestroyCommandPool(mLogicalDevice, mCommandPool, nullptr);
        if (mLogicalDevice) vkDestroyDevice(mLogicalDevice, nullptr);
    }

    /**
	* Get the index of a memory type that has all the requested property bits set
	*
	* @param typeBits Bit mask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
	* @param memProps Bit mask of properties for the memory type to request
	* @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
	*
	* @return Index of the requested memory type
	*
	* @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
	*/
    uint32_t VulkanDevice::GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags memProps, VkBool32 *memTypeFound) const
    {
        for (uint32_t i = 0; i < mMemoryProperties.memoryTypeCount; i++)
        {
            if ((typeBits & 1) == 1)
            {
                if ((mMemoryProperties.memoryTypes[i].propertyFlags & memProps) == memProps)
                {
                    if (memTypeFound) *memTypeFound = true;
                    return 1;
                }
            }
            typeBits >>= 1;
        }
        if (memTypeFound)
        {
            *memTypeFound = false;
            return 0;
        }
        else
        {
            throw std::runtime_error("Could not find a matching memory type");
        }
    }

    /**
	* Get the index of a queue family that supports the requested queue flags
	* SRS - support VkQueueFlags parameter for requesting multiple flags vs. VkQueueFlagBits for a single flag only
	*
	* @param queueFlags Queue flags to find a queue family index for
	*
	* @return Index of the queue family index that matches the flags
	*
	* @throw Throws an exception if no queue family index could be found that supports the requested flags
	*/
    uint32_t VulkanDevice::GetQueueFamilyIndex(VkQueueFlags queueFlags) const
    {
        // Compute专用队列
        // 找到一个Queue Family Index只支持Compute不支持Graphics
        if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(mQueueFamilyProperties.size()); i++)
            {
                if ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                    ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
                {
                    return i;
                }
            }
        }

        // Transfer专用队列
        // 找到一个Queue Family Index只支持Transfer
        if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(mQueueFamilyProperties.size()); i++)
            {
                if ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                    ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
                    ((mQueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
                {
                    return i;
                }
            }
        }

        // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
        for (uint32_t i = 0; i < static_cast<uint32_t>(mQueueFamilyProperties.size()); i++)
        {
            if ((mQueueFamilyProperties[i].queueFlags & queueFlags) == queueFlags)
            {
                return i;
            }
        }

        throw std::runtime_error("Could not find a matching queue family index");
    }

    /**
	* Create the logical device based on the assigned physical device, also gets default queue family indices
	*
	* @param enabledFeatures Can be used to enable certain features upon device creation
    * @param enabledExtensions Device启用的扩展
	* @param pNextChain Optional chain of pointer to extension structures
	* @param useSwapChain Set to false for headless rendering to omit the swapchain device extensions
	* @param requestedQueueTypes Bit flags specifying the queue types to be requested from the device
	*
	* @return VkResult of the device creation call
	*/
    VkResult VulkanDevice::CreateLogicalDevice(
        VkPhysicalDeviceFeatures enabledFeatures,
        std::vector<const char *> enabledExtensions,
        void *pNextChain,
        bool useSwapChain,
        VkQueueFlags requestedQueueTypes)
    {
        std::vector<VkDeviceQueueCreateInfo> queueCIs{};

        // 根据需求的队列类型获取队列索引
        const float defaultQueuePriority(0.0f);

        // Graphics Queue
        if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
        {
            mQueueFamilyIndices.graphics = GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
            VkDeviceQueueCreateInfo queueCI{};
            queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCI.queueFamilyIndex = mQueueFamilyIndices.graphics;
            queueCI.queueCount = 1;
            queueCI.pQueuePriorities = &defaultQueuePriority;
            queueCIs.push_back(queueCI);
        }
        else
        {
            mQueueFamilyIndices.graphics = 0;
        }
        // Compute Queue
        if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
        {
            mQueueFamilyIndices.compute = GetQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
            if (mQueueFamilyIndices.compute != mQueueFamilyIndices.graphics)
            {
                VkDeviceQueueCreateInfo queueCI{};
                queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCI.queueFamilyIndex = mQueueFamilyIndices.compute;
                queueCI.queueCount = 1;
                queueCI.pQueuePriorities = &defaultQueuePriority;
                queueCIs.push_back(queueCI);
            }
        }
        else
        {
            mQueueFamilyIndices.compute = mQueueFamilyIndices.graphics;
        }
        // Transfer Queue
        if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
        {
            mQueueFamilyIndices.transfer = GetQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
            if (mQueueFamilyIndices.transfer != mQueueFamilyIndices.graphics &&
                mQueueFamilyIndices.transfer != mQueueFamilyIndices.compute)
            {
                VkDeviceQueueCreateInfo queueCI{};
                queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCI.queueFamilyIndex = mQueueFamilyIndices.transfer;
                queueCI.queueCount = 1;
                queueCI.pQueuePriorities = &defaultQueuePriority;
                queueCIs.push_back(queueCI);
            }
        }
        else
        {
            mQueueFamilyIndices.transfer = mQueueFamilyIndices.graphics;
        }

        // Create Logical Device
        std::vector<const char*> deviceExtensions(enabledExtensions);
        if (useSwapChain)
        {
            deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        VkDeviceCreateInfo deviceCI{};
        deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCI.queueCreateInfoCount = static_cast<uint32_t>(queueCIs.size());;
        deviceCI.pQueueCreateInfos = queueCIs.data();
        deviceCI.pEnabledFeatures = &enabledFeatures;

        // If a pNext(Chain) has been passed, we need to add it to the device creation info
        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
        if (pNextChain)
        {
            physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physicalDeviceFeatures2.features = enabledFeatures;
            physicalDeviceFeatures2.pNext = pNextChain;
            deviceCI.pEnabledFeatures = nullptr;
            deviceCI.pNext = &physicalDeviceFeatures2;
        }

        // Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
        if (ExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        {
            deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
            mbEnableDebugMarkers = true;
        }

        if (!deviceExtensions.empty())
        {
            for (const char* enabledExtension : deviceExtensions)
            {
                if (!ExtensionSupported(enabledExtension))
                {
                    std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
                }
            }

            deviceCI.enabledExtensionCount = (uint32_t)deviceExtensions.size();
            deviceCI.ppEnabledExtensionNames = deviceExtensions.data();
        }

        mEnabledFeatures = enabledFeatures;

        VkResult res = vkCreateDevice(mPhysicalDevice, &deviceCI, nullptr, &mLogicalDevice);
        if (res != VK_SUCCESS) return res;

        mCommandPool = CreateCommandPool(mQueueFamilyIndices.graphics);

        return res;
    }

    /**
    * Create a buffer on the device
    *
    * @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
    * @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
    * @param size Size of the buffer in byes
    * @param buffer Pointer to the buffer handle acquired by the function
    * @param memory Pointer to the memory handle acquired by the function
    * @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
    *
    * @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
    */
    VkResult VulkanDevice::CreateBuffer(
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize size,
        VkBuffer *buffer,
        VkDeviceMemory *memory, void *data)
    {
        // Buffer Handle
        VkBufferCreateInfo bufferCI = LeoVK::Init::BufferCreateInfo(usageFlags, size);
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(mLogicalDevice, &bufferCI, nullptr, buffer))

        VkMemoryRequirements memReqs;
        VkMemoryAllocateInfo memAI = LeoVK::Init::MemoryAllocateInfo();
        vkGetBufferMemoryRequirements(mLogicalDevice, *buffer, &memReqs);
        memAI.allocationSize = memReqs.size;
        memAI.memoryTypeIndex = GetMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
        if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            memAI.pNext = &allocFlagsInfo;
        }
        VK_CHECK(vkAllocateMemory(mLogicalDevice, &memAI, nullptr, memory))

        // 如果传入了Buffer的指针，那么就映射Buffer并拷贝数据
        if (data != nullptr)
        {
            void *mapped;
            VK_CHECK(vkMapMemory(mLogicalDevice, *memory, 0, size, 0, &mapped))
            memcpy(mapped, data, size);
            // If host coherency hasn't been requested, do a manual flush to make writes visible
            if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            {
                VkMappedMemoryRange mappedRange = LeoVK::Init::MappedMemoryRange();
                mappedRange.memory = *memory;
                mappedRange.offset = 0;
                mappedRange.size = size;
                vkFlushMappedMemoryRanges(mLogicalDevice, 1, &mappedRange);
            }
            vkUnmapMemory(mLogicalDevice, *memory);
        }
        VK_CHECK(vkBindBufferMemory(mLogicalDevice, *buffer, *memory, 0))
        return VK_SUCCESS;
    }

    /**
	* Create a buffer on the device
	*
	* @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
	* @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
	* @param buffer Pointer to a vk::Vulkan buffer object
	* @param size Size of the buffer in bytes
	* @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
	*
	* @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
	*/
    VkResult VulkanDevice::CreateBuffer(
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        LeoVK::Buffer *buffer,
        VkDeviceSize size,
        void *data)
    {
        buffer->mDevice = mLogicalDevice;

        VkBufferCreateInfo bufferCreateInfo = LeoVK::Init::BufferCreateInfo(usageFlags, size);
        VK_CHECK(vkCreateBuffer(mLogicalDevice, &bufferCreateInfo, nullptr, &buffer->mBuffer));

        // Create the memory backing up the buffer handle
        VkMemoryRequirements memReqs;
        VkMemoryAllocateInfo memAlloc = LeoVK::Init::MemoryAllocateInfo();
        vkGetBufferMemoryRequirements(mLogicalDevice, buffer->mBuffer, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = GetMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
        if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            memAlloc.pNext = &allocFlagsInfo;
        }
        VK_CHECK(vkAllocateMemory(mLogicalDevice, &memAlloc, nullptr, &buffer->mMemory));

        buffer->mAlignment = memReqs.alignment;
        buffer->mSize = size;
        buffer->mUsageFlags = usageFlags;
        buffer->mMemPropFlags = memoryPropertyFlags;

        // If a pointer to the buffer data has been passed, map the buffer and copy over the data
        if (data != nullptr)
        {
            VK_CHECK(buffer->Map());
            memcpy(buffer->mpMapped, data, size);
            if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
                buffer->Flush();

            buffer->UnMap();
        }

        // Initialize a default descriptor that covers the whole buffer size
        buffer->SetupDescriptor();

        // Attach the memory to the buffer object
        return buffer->Bind();
    }

    /**
	* Copy buffer data from src to dst using VkCmdCopyBuffer
	*
	* @param src Pointer to the source buffer to copy from
	* @param dst Pointer to the destination buffer to copy to
	* @param queue Pointer
	* @param copyRegion (Optional) Pointer to a copy region, if NULL, the whole buffer is copied
	*
	* @note Source and destination pointers must have the appropriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST)
	*/
    void VulkanDevice::CopyBuffer(
        LeoVK::Buffer *src,
        LeoVK::Buffer *dst,
        VkQueue queue,
        VkBufferCopy *copyRegion)
    {
        assert(dst->mSize <= src->mSize);
        assert(src->mBuffer);
        VkCommandBuffer copyCmd = CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy bufferCopy{};
        if (copyRegion == nullptr)
        {
            bufferCopy.size = src->mSize;
        }
        else
        {
            bufferCopy = *copyRegion;
        }
        vkCmdCopyBuffer(copyCmd, src->mBuffer, dst->mBuffer, 1, &bufferCopy);

        FlushCommandBuffer(copyCmd, queue);
    }

    /**
	* Create a command pool for allocation command buffers from
	*
	* @param queueFamilyIndex Family index of the queue to create the command pool for
	* @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
	*
	* @note Command buffers allocated from the created pool can only be submitted to a queue with the same family index
	*
	* @return A handle to the created command buffer
	*/
    VkCommandPool VulkanDevice::CreateCommandPool(
        uint32_t queueFamilyIndex,
        VkCommandPoolCreateFlags createFlags)
    {
        VkCommandPoolCreateInfo cmdPoolCI {};
        cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCI.queueFamilyIndex = queueFamilyIndex;
        cmdPoolCI.flags = createFlags;
        VkCommandPool cmdPool;
        VK_CHECK(vkCreateCommandPool(mLogicalDevice, &cmdPoolCI, nullptr, &cmdPool))
        return cmdPool;
    }

    /**
	* Allocate a command buffer from the command pool
	*
	* @param level Level of the new command buffer (primary or secondary)
	* @param pool Command pool from which the command buffer will be allocated
	* @param (Optional) begin If true, recording on the new command buffer will be started (vkBeginCommandBuffer) (Defaults to false)
	*
	* @return A handle to the allocated command buffer
	*/
    VkCommandBuffer VulkanDevice::CreateCommandBuffer(
        VkCommandBufferLevel level,
        VkCommandPool pool, bool begin)
    {
        VkCommandBufferAllocateInfo cmdBufferAI = LeoVK::Init::CmdBufferAllocateInfo(pool, level, 1);
        VkCommandBuffer cmdBuffer;
        VK_CHECK(vkAllocateCommandBuffers(mLogicalDevice, &cmdBufferAI, &cmdBuffer));
        // If requested, also start recording for the new command buffer
        if (begin)
        {
            VkCommandBufferBeginInfo cmdBufInfo = LeoVK::Init::CmdBufferBeginInfo();
            VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
        }
        return cmdBuffer;
    }

    VkCommandBuffer VulkanDevice::CreateCommandBuffer(VkCommandBufferLevel level, bool begin)
    {
        return CreateCommandBuffer(level, mCommandPool, begin);
    }

    void VulkanDevice::BeginCommandBuffer(VkCommandBuffer commandBuffer)
    {
        VkCommandBufferBeginInfo commandBufferBI{};
        commandBufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBI));
    }

    /**
	* Finish command buffer recording and submit it to a queue
	*
	* @param commandBuffer Command buffer to flush
	* @param queue Queue to submit the command buffer to
	* @param pool Command pool on which the command buffer has been created
	* @param free (Optional) Free the command buffer once it has been submitted (Defaults to true)
	*
	* @note The queue that the command buffer is submitted to must be from the same family index as the pool it was allocated from
	* @note Uses a fence to ensure command buffer has finished executing
	*/
    void VulkanDevice::FlushCommandBuffer(
        VkCommandBuffer commandBuffer,
        VkQueue queue,
        VkCommandPool pool,
        bool free)
    {
        if (commandBuffer == VK_NULL_HANDLE) return;

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo = LeoVK::Init::SubmitInfo();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fenceInfo = LeoVK::Init::FenceCreateInfo(VK_FLAGS_NONE);
        VkFence fence;
        VK_CHECK(vkCreateFence(mLogicalDevice, &fenceInfo, nullptr, &fence));
        // Submit to the queue
        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
        // Wait for the fence to signal that command buffer has finished executing
        VK_CHECK(vkWaitForFences(mLogicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
        vkDestroyFence(mLogicalDevice, fence, nullptr);
        if (free)
        {
            vkFreeCommandBuffers(mLogicalDevice, pool, 1, &commandBuffer);
        }
    }

    void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
    {
        return FlushCommandBuffer(commandBuffer, queue, mCommandPool, free);
    }

    bool VulkanDevice::ExtensionSupported(std::string extension)
    {
        return (std::find(mSupportedExtensions.begin(), mSupportedExtensions.end(), extension) != mSupportedExtensions.end());
    }

    VkFormat VulkanDevice::GetSupportedDepthFormat(bool checkSamplingSupport)
    {
        // All depth formats may be optional, so we need to find a suitable depth format to use
        std::vector<VkFormat> depthFormats = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM };

        for (auto& format : depthFormats)
        {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &formatProperties);
            // Format must support depth stencil attachment for optimal tiling
            if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                if (checkSamplingSupport)
                {
                    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) continue;
                }
                return format;
            }
        }
        throw std::runtime_error("Could not find a matching depth format");
    }


}