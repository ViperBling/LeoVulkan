#pragma once

#include <VKTypes.h>

class LeoVKEngine
{
public:

    void Init();
    void CleanUp();
    void Draw();
    void Run();

public:
    bool bIsInitialized {false};
    int mFrameNumber {0};

    VkExtent2D mWndExtent {1280, 720};

    struct SDL_Window* mWindow {nullptr};

    VkInstance mInstance;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;
    VkSurfaceKHR mSurface;

    VkSwapchainKHR mSwapChain;
    VkFormat mSwapChainImageFormat;
    std::vector<VkImage> mSwapChainImages;
    std::vector<VkImageView> mSwapChainImageViews;


private:

    void initVulkan();
    void initSwapChain();
};