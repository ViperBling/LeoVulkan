#pragma once

#include <vector>

#include "VKTypes.hpp"

enum class ImageTransitionMode
{
    IntoAttachment,
    IntoGeneral,
    GeneralToPresent,
    AttachmentToPresent
};

class VulkanEngine
{
public:
    void Init();
    void CleanUp();
    void Draw();
    void Run();

private:
    void initVulkan();
    void initSwapChain();
    void initDefaultRenderPass();
    void initFrameBuffers();
    void initCommands();
    void initSyncObjects();
    void transitionImage(VkCommandBuffer cmdBuffer, VkImage image, ImageTransitionMode transitionMode);

public:
    bool mb_Initialized {false};
    int mFrameIndex {0};

    VkExtent2D mWndExtent {1280, 720};
    struct SDL_Window* mWnd {nullptr};

    VkInstance mInstance;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    VkPhysicalDevice mGPU;
    VkDevice mDevice;

    VkSemaphore mPresentSem, mRenderSem;
    VkFence mRenderFence;

    VkQueue mGraphicsQueue;
    uint32_t mGraphicsQueueFamily;

    VkCommandPool mCmdPool;
    VkCommandBuffer mCmdBuffer;

    VkRenderPass mRenderPass;

    VkSurfaceKHR mSurface;
    VkSwapchainKHR mSwapChain;
    VkFormat mSwapChainFormat;

    std::vector<VkFramebuffer> mFrameBuffers;
    std::vector<VkImage> mSwapChainImages;
    std::vector<VkImageView> mSwapChainImageViews;
};