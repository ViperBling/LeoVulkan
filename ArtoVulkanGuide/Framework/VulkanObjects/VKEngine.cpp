#include <iostream>
#include <array>
#include <fstream>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#include "VKEngine.hpp"
#include "VKTypes.hpp"
#include "VKInitializers.hpp"
#include "VKImage.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

constexpr bool bUseValidationLayers = true;

void VulkanEngine::Init()
{
    SDL_Init(SDL_INIT_VIDEO);

    auto wndFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    mWnd = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        (int)mWndExtent.width,
        (int)mWndExtent.height,
        wndFlags
        );

    initVulkan();
    initSwapChain();
    initCommands();
    initSyncObjects();
    initDescriptors();
    initPipelines();

    mb_Initialized = true;
}

void VulkanEngine::CleanUp()
{
    if (mb_Initialized)
    {
        vkDeviceWaitIdle(mDevice);
        vkDestroyCommandPool(mDevice, mCmdPool, nullptr);

        //destroy sync objects
        vkDestroyFence(mDevice, mRenderFence, nullptr);
        vkDestroySemaphore(mDevice, mRenderSem, nullptr);
        vkDestroySemaphore(mDevice, mPresentSem, nullptr);

        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);

        //destroy swapchain resources
        for (auto & mSwapChainImageView : mSwapChainImageViews) {

            vkDestroyImageView(mDevice, mSwapChainImageView, nullptr);
        }

        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

        vkDestroyDevice(mDevice, nullptr);
        vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
        vkDestroyInstance(mInstance, nullptr);

        SDL_DestroyWindow(mWnd);
    }
}

void VulkanEngine::Draw()
{
    // 等GPU渲染完成最后一帧，超时1s
    VK_CHECK(vkWaitForFences(mDevice, 1, &mRenderFence, true, 1000000000));
    VK_CHECK(vkResetFences(mDevice, 1, &mRenderFence));

    // 等到Fence同步后，可以确定命令执行完成，可以重置Command Buffer
    VK_CHECK(vkResetCommandBuffer(mCmdBuffer, 0));

    // 从SwapChain中获取Image的Index
    uint32_t swapChainImageIdx;
    VK_CHECK(vkAcquireNextImageKHR(mDevice, mSwapChain, 1000000000, mPresentSem, nullptr, &swapChainImageIdx));

    // 开始Command Buffer的录制，这里只使用一次用于显示
    VkCommandBufferBeginInfo cmdBI = VKInit::CmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(mCmdBuffer, &cmdBI));

    // 在渲染前让Image变得可写
    VKUtil::TransitionImage(mCmdBuffer, mDrawImage.mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // 绑定管线
    vkCmdBindPipeline(mCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline);

    vkCmdBindDescriptorSets(mCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mPipelineLayout, 0, 1, &mDrawImageDesc, 0, nullptr);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(mCmdBuffer,std::ceil(mWndExtent.width / 16.0), std::ceil(mWndExtent.height / 16.0),1);

    VKUtil::TransitionImage(mCmdBuffer, mDrawImage.mImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VKUtil::TransitionImage(mCmdBuffer, mSwapChainImages[swapChainImageIdx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkExtent3D extent;
    extent.height = mWndExtent.height;
    extent.width = mWndExtent.width;
    extent.depth = 1;

    // execute a copy from the draw image into the swapchain
    VKUtil::CopyImageToImage(mCmdBuffer, mDrawImage.mImage, mSwapChainImages[swapChainImageIdx], extent);

    // set swapchain image layout to Present so we can show it on the screen
    VKUtil::TransitionImage(mCmdBuffer, mSwapChainImages[swapChainImageIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // 所有操作完成，可以关闭CommandBuffer，不能写入了，可以开始执行
    VK_CHECK(vkEndCommandBuffer(mCmdBuffer));

    // 准备提交CommandBuffer到队列
    // 需要在wait信号量上等待，它表示交换链在渲染前准备完毕
    // 渲染完成后需要signal信号量
    VkCommandBufferSubmitInfo cmdBufferSI = VKInit::CmdBufferSubmitInfo(mCmdBuffer);

    VkSemaphoreSubmitInfo waitInfo = VKInit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, mPresentSem);
    VkSemaphoreSubmitInfo signalInfo = VKInit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, mRenderSem);

    VkSubmitInfo2 submit = VKInit::SubmitInfo2(&cmdBufferSI, &signalInfo, &waitInfo);

    // 提交命令到队列并执行
    // renderFence会阻塞直到命令执行完成
    VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submit, mRenderFence));

    // 准备呈现，把刚才渲染的图片呈现到窗口
    // 现在要等的是render信号量，确保渲染完成后才提交到窗口
    VkPresentInfoKHR presentInfo = VKInit::PresentInfo();
    presentInfo.pSwapchains = &mSwapChain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &mRenderSem;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapChainImageIdx;

    VK_CHECK(vkQueuePresentKHR(mGraphicsQueue, &presentInfo));

    mFrameIndex++;
}

void VulkanEngine::Run()
{
    SDL_Event event;
    bool bQuit = false;

    while (!bQuit)
    {
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT) bQuit = true;
        }
        Draw();
    }
}

void VulkanEngine::initVulkan()
{
    vkb::InstanceBuilder builder;

    auto inst = builder.set_app_name("Vulkan Engine")
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkbInst = inst.value();

    mInstance = vkbInst.instance;
    mDebugMessenger = vkbInst.debug_messenger;

    SDL_Vulkan_CreateSurface(mWnd, mInstance, &mSurface);

    VkPhysicalDeviceVulkan13Features features{};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    vkb::PhysicalDeviceSelector gpuSelector{ vkbInst };
    vkb::PhysicalDevice physicalDevice = gpuSelector
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .set_surface(mSurface)
        .select()
        .value();

    vkb::DeviceBuilder deviceBuilder { physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    mDevice = vkbDevice.device;
    mGPU = physicalDevice.physical_device;

    mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // 初始化分配器
    VmaAllocatorCreateInfo vmaAllocatorCI {};
    vmaAllocatorCI.physicalDevice = mGPU;
    vmaAllocatorCI.device = mDevice;
    vmaAllocatorCI.instance = mInstance;
    vmaCreateAllocator(&vmaAllocatorCI, &mAllocator);

    mMainDeletionQueue.PushFunction([&]()
    {
        vmaDestroyAllocator(mAllocator);
    });
}

void VulkanEngine::initSwapChain()
{
    vkb::SwapchainBuilder swapchainBuilder { mGPU, mDevice, mSurface };
    vkb::Swapchain vkbSwapChain = swapchainBuilder
        .set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
        .set_desired_extent(mWndExtent.width, mWndExtent.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    mSwapChain = vkbSwapChain.swapchain;
    mSwapChainImages = vkbSwapChain.get_images().value();
    mSwapChainImageViews = vkbSwapChain.get_image_views().value();
    mSwapChainFormat = vkbSwapChain.image_format;

    VkExtent3D drawImageExtent = {
        mWndExtent.width,
        mWndExtent.height,
        1
    };

    mDrawFormat = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageUsageFlags drawImageUsage{};
    drawImageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;

    VkImageCreateInfo imgCI = VKInit::ImageCreateInfo(mDrawFormat, drawImageUsage, drawImageExtent);

    VmaAllocationCreateInfo imgAllocCI {};
    imgAllocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imgAllocCI.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(mAllocator, &imgCI, &imgAllocCI, &mDrawImage.mImage, &mDrawImage.mAllocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo imgViewCI = VKInit::ImageViewCreateInfo(mDrawFormat, mDrawImage.mImage, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(mDevice, &imgViewCI, nullptr, &mDrawImageView));

    //add to deletion queues
    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroyImageView(mDevice, mDrawImageView, nullptr);
        vmaDestroyImage(mAllocator, mDrawImage.mImage, mDrawImage.mAllocation);
    });
}

void VulkanEngine::initDefaultRenderPass()
{

}

void VulkanEngine::initFrameBuffers()
{

}

void VulkanEngine::initCommands()
{
    VkCommandPoolCreateInfo cmdPoolCI = VKInit::CmdPoolCreateInfo(mGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(mDevice, &cmdPoolCI, nullptr, &mCmdPool));

    VkCommandBufferAllocateInfo cmdBufferAI = VKInit::CmdBufferAllocateInfo(mCmdPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdBufferAI, &mCmdBuffer));
}

void VulkanEngine::initPipelines()
{
    VkShaderModule computeDraw;
    if (!loadShaderModule("../../Shaders/Gradient.comp.spv", &computeDraw))
    {
        std::cout << "Error when building the colored mesh shader" << std::endl;
    }

    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &mSwapChainImageDescLayout;
    computeLayout.setLayoutCount = 1;


    VK_CHECK(vkCreatePipelineLayout(mDevice, &computeLayout, nullptr, &mPipelineLayout));

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = computeDraw;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = mPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(vkCreateComputePipelines(mDevice,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &mPipeline));
}

void VulkanEngine::initDescriptors()
{
    //create a descriptor pool that will hold 10 uniform buffers
    std::vector<VkDescriptorPoolSize> sizes =
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 }
        };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0;
    pool_info.maxSets = 10;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();

    vkCreateDescriptorPool(mDevice, &pool_info, nullptr, &mDescPool);

    VkDescriptorSetLayoutBinding imageBind = VKInit::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0);

    VkDescriptorSetLayoutBinding bindings[] = { imageBind };

    VkDescriptorSetLayoutCreateInfo setinfo = {};
    setinfo.bindingCount = 1;
    setinfo.flags = 0;
    setinfo.pNext = nullptr;
    setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setinfo.pBindings = bindings;

    vkCreateDescriptorSetLayout(mDevice, &setinfo, nullptr, &mSwapChainImageDescLayout);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mSwapChainImageDescLayout;

    vkAllocateDescriptorSets(mDevice, &allocInfo, &mDrawImageDesc);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = mDrawImageView;

    VkWriteDescriptorSet cameraWrite = VKInit::WriteDescImage(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, mDrawImageDesc, &imgInfo, 0);

    vkUpdateDescriptorSets(mDevice, 1, &cameraWrite, 0, nullptr);
}

void VulkanEngine::initSyncObjects()
{
    VkFenceCreateInfo fenceCI = VKInit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(mDevice, &fenceCI, nullptr, &mRenderFence));

    VkSemaphoreCreateInfo semCI = VKInit::SemaphoreCreateInfo();
    VK_CHECK(vkCreateSemaphore(mDevice, &semCI, nullptr, &mPresentSem));
    VK_CHECK(vkCreateSemaphore(mDevice, &semCI, nullptr, &mRenderSem));
}

bool VulkanEngine::loadShaderModule(const char *filepath, VkShaderModule *outShaderModule)
{
    //open the file. With cursor at the end
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    //find what the size of the file is by looking up the location of the cursor
    //because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();

    //spirv expects the buffer to be on uint32, so make sure to reserve a int vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    //put file cursor at beggining
    file.seekg(0);

    //load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    //now that the file is loaded into the buffer, we can close it
    file.close();

    //create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo smCI = {};
    smCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smCI.pNext = nullptr;

    //codeSize has to be in bytes, so multply the ints in the buffer by size of int to know the real size of the buffer
    smCI.codeSize = buffer.size() * sizeof(uint32_t);
    smCI.pCode = buffer.data();

    //check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(mDevice, &smCI, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return false;
    }

    *outShaderModule = shaderModule;
    return true;
}


