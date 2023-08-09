#pragma once

#include <vector>
#include <deque>
#include <iostream>
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

struct Material
{
    VkPipeline mPipeline;
    VkPipelineLayout  mPipelineLayout;
};

struct RenderScene
{
    Mesh* mMesh;
    Material* mMaterial;
    glm::mat4 mTransformMatrix;
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
    void initScene();
    // 创建同步对象，一个Fence用于控制GPU合适完成渲染
    // 两个信号量来同步渲染和SwapChain
    void initSyncObjects();
    bool loadShaderModule(const char* filepath, VkShaderModule* outShaderModule);
    void loadMeshes();
    void uploadMesh(Mesh& mesh);

    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout pipelineLayout, const std::string& name);
    Material* getMaterial(const std::string& name);
    Mesh* getMesh(const std::string& name);

    void drawObjects(VkCommandBuffer cmdBuffer, RenderScene* first, uint32_t count);

public:
    bool mb_Initialized {false};
    int mFrameIndex {0};
    int mSelectedShader {0};

    VkExtent2D mWndExtent {1024, 576};
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

    DeletionQueue mMainDeletionQueue;

    VmaAllocator mAllocator;

    VkImageView mDSView;
    AllocatedImage mDSImage;
    VkFormat mDSFormat;

    std::vector<RenderScene> mRenderScenes;
    std::unordered_map<std::string, Material> mMaterials;
    std::unordered_map<std::string, Mesh> mMeshes;
};