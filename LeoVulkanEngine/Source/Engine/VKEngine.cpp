#include "VkEngine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <VKTypes.h>
#include <VKPCH.h>
#include <VKInitializers.h>

#include <VkBootstrap.h>

void LeoVKEngine::Init()
{
    SDL_Init(SDL_INIT_VIDEO);

    auto wndFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    mWindow = SDL_CreateWindow(
        "LeoVKEngine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        (int)mWndExtent.width,
        (int)mWndExtent.height,
        wndFlags
        );

    // 初始化Vulkan结构
    initVulkan();
    // 初始化交换链
    initSwapChain();

    bIsInitialized = true;
}

void LeoVKEngine::CleanUp()
{
    if (bIsInitialized)
    {
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);

        for (int i = 0; i < mSwapChainImages.size(); i++)
        {
            vkDestroyImageView(mDevice, mSwapChainImageViews[i], nullptr);
        }
        vkDestroyDevice(mDevice, nullptr);
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
        vkDestroyInstance(mInstance, nullptr);

        SDL_DestroyWindow(mWindow);
    }
}

void LeoVKEngine::Draw()
{

}

void LeoVKEngine::Run()
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

void LeoVKEngine::initVulkan()
{
    vkb::InstanceBuilder builder;

    // 构造Vulkan Instance
    auto inst = builder.set_app_name("LeoVKEngine")
        .request_validation_layers(true)
        .require_api_version(1, 1, 0)
        .use_default_debug_messenger()
        .build();

    vkb::Instance vkbInst = inst.value();

    mInstance = vkbInst.instance;
    mDebugMessenger = vkbInst.debug_messenger;

    // 获取SDL中的Surface
    SDL_Vulkan_CreateSurface(mWindow, mInstance, &mSurface);

    // 使用vkbootstrap选择GPU
    vkb::PhysicalDeviceSelector selector {vkbInst};
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 1)
        .set_surface(mSurface)
        .select()
        .value();

    // 创建Vulkan Device
    vkb::DeviceBuilder deviceBuilder {physicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    mDevice = vkbDevice.device;
    mPhysicalDevice = physicalDevice.physical_device;
}

void LeoVKEngine::initSwapChain()
{
    vkb::SwapchainBuilder scBuilder {mPhysicalDevice, mDevice, mSurface};

    vkb::Swapchain vkbSwapChain = scBuilder
        .use_default_format_selection()
        // 垂直同步
        .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
        .set_desired_extent(mWndExtent.width, mWndExtent.height)
        .build()
        .value();

    mSwapChain = vkbSwapChain.swapchain;
    mSwapChainImages = vkbSwapChain.get_images().value();
    mSwapChainImageViews = vkbSwapChain.get_image_views().value();
    mSwapChainImageFormat = vkbSwapChain.image_format;
}
