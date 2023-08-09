#pragma once

#include <vector>
#include <deque>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "VKTypes.hpp"
#include "VKPipeline.hpp"
#include "VKMesh.hpp"

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

struct MeshPushConstants
{
    glm::vec4 mData;
    glm::mat4 mMat;
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
    void initPipelines();
    // 创建同步对象，一个Fence用于控制GPU合适完成渲染
    // 两个信号量来同步渲染和SwapChain
    void initSyncObjects();
    bool loadShaderModule(const char* filepath, VkShaderModule* outShaderModule);
    void loadMeshes();
    void uploadMesh(Mesh& mesh);

public:
    bool mb_Initialized {false};
    int mFrameIndex {0};
    int mSelectedShader {0};

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

    VkPipeline mTrianglePipeline;
    VkPipeline mRedTrianglePipeline;
    VkPipeline mMeshPipeline;
    VkPipelineLayout mTrianglePipelineLayout;
    VkPipelineLayout mMeshPipelineLayout;

    DeletionQueue mMainDeletionQueue;

    Mesh mTriangleMesh;
    Mesh mMesh;

    VmaAllocator mAllocator;

    VkImageView mDSView;
    AllocatedImage mDSImage;
    VkFormat mDSFormat;
};