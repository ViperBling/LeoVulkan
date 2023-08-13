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

constexpr unsigned int FRAME_OVERLAP = 2;

struct MeshPushConstants
{
    glm::vec4 mData;
    glm::mat4 mMatrix;
};

struct Material
{
    VkDescriptorSet mTexSet = VK_NULL_HANDLE;
    VkPipeline mPipeline = VK_NULL_HANDLE;;
    VkPipelineLayout  mPipelineLayout = VK_NULL_HANDLE;;
};

struct Texture
{
    AllocatedImage mImage;
    VkImageView mImageView;
};

struct RenderScene
{
    Mesh* mMesh;
    Material* mMaterial;
    glm::mat4 mTransform;
};

struct FrameData
{
    VkSemaphore mPresentSem, mRenderSem;
    VkFence mRenderFence;

    DeletionQueue mFrameDeletionQueue;

    VkCommandPool mCmdPool;
    VkCommandBuffer mCmdBuffer;

    AllocatedBuffer mCameraBuffer;
    VkDescriptorSet mGlobalDescSet;

    AllocatedBuffer mSceneBuffer;
    VkDescriptorSet mSceneDescSet;
};

struct UploadContext
{
    VkFence mUploadFence;
    VkCommandPool mCmdPool;
    VkCommandBuffer mCmdBuffer;
};

struct GPUCameraData
{
    glm::mat4 mView;
    glm::mat4 mProj;
    glm::mat4 mVP;
};

struct UniformData
{
    glm::vec4 mFogColor;            // w is for exponent
    glm::vec4 mFogDistance;         //x for min, y for max, zw unused.
    glm::vec4 mAmbientColor;
    glm::vec4 mSunLightDirection;   //w for sun power
    glm::vec4 mSunLightColor;
};

struct GPUObjectData
{
    glm::mat4 mModelMatrix;
};

class VulkanEngine
{
public:
    void Init();
    void CleanUp();
    void Draw();
    void Run();

    FrameData& GetCurrentFrame();
    FrameData& GetLastFrame();

    Material* CreateMaterial(VkPipeline pipeline, VkPipelineLayout pipelineLayout, const std::string& name);
    Material* GetMaterial(const std::string& name);
    Mesh* GetMesh(const std::string& name);
    void DrawObjects(VkCommandBuffer cmdBuffer, RenderScene* first, uint32_t count);

    AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    size_t PadUniformBufferSize(size_t originalSize);

    void ImmediateSubmit(std::function<void(VkCommandBuffer cmdBuffer)>&& function);

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
    void initDescriptors();

    bool loadShaderModule(const char* filepath, VkShaderModule* outShaderModule);
    void loadMeshes();
    void uploadMesh(Mesh& mesh);
    void loadImages();

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
    VkPhysicalDeviceProperties mGPUProps;

    FrameData mFrames[FRAME_OVERLAP];

    VkQueue mGraphicsQueue;
    uint32_t mGraphicsQueueFamily;

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
    std::unordered_map<std::string, Texture> mTextures;

    VkDescriptorPool mDescPool;
    VkDescriptorSetLayout mGlobalDescSetLayout;
    VkDescriptorSetLayout mSceneDescSetLayout;
    VkDescriptorSetLayout mTextureDescSetLayout;

    UniformData mUniformParams;
    AllocatedBuffer mUniformBuffer;

    UploadContext mUploadContext;
};