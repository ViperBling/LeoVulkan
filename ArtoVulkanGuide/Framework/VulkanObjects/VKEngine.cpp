#include <iostream>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#include "VKEngine.hpp"
#include "VKTypes.hpp"
#include "VKInitializers.hpp"

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

    VkClearColorValue clearValue;
    float flash = abs(sin((float)mFrameIndex / 120.f));
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    // 在渲染前让Image变得可写
    transitionImage(mCmdBuffer, mSwapChainImages[swapChainImageIdx], ImageTransitionMode::IntoGeneral);

    VkImageSubresourceRange clearRange = VKInit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(mCmdBuffer, mSwapChainImages[swapChainImageIdx], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    // 写入东西后，让Image状态变成准备Present
    transitionImage(mCmdBuffer, mSwapChainImages[swapChainImageIdx], ImageTransitionMode::GeneralToPresent);

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

void VulkanEngine::initSyncObjects()
{
    VkFenceCreateInfo fenceCI = VKInit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(mDevice, &fenceCI, nullptr, &mRenderFence));

    VkSemaphoreCreateInfo semCI = VKInit::SemaphoreCreateInfo();
    VK_CHECK(vkCreateSemaphore(mDevice, &semCI, nullptr, &mPresentSem));
    VK_CHECK(vkCreateSemaphore(mDevice, &semCI, nullptr, &mRenderSem));
}

void VulkanEngine::transitionImage(VkCommandBuffer cmdBuffer, VkImage image, ImageTransitionMode transitionMode)
{
    VkImageMemoryBarrier2 imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    imageBarrier.srcAccessMask = VK_ACCESS_2_NONE_KHR;
    imageBarrier.dstAccessMask = VK_ACCESS_2_NONE_KHR;

    switch (transitionMode)
    {
        case ImageTransitionMode::IntoAttachment:
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            break;

        case ImageTransitionMode::AttachmentToPresent:
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
        case ImageTransitionMode::IntoGeneral:
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case ImageTransitionMode::GeneralToPresent:
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
    }

    imageBarrier.image = image;
    imageBarrier.subresourceRange = VKInit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    VkDependencyInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    info.pNext = nullptr;
    info.imageMemoryBarrierCount = 1;
    info.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmdBuffer, &info);
}
