#include <iostream>
#include <array>
#include <fstream>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "VKEngine.hpp"
#include "VKTypes.hpp"
#include "VKInitializers.hpp"
#include "VKImage.hpp"

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
    initDefaultRenderPass();
    initFrameBuffers();
    initCommands();
    initSyncObjects();
    initPipelines();
    loadMeshes();
    initScene();

    mb_Initialized = true;
}

void VulkanEngine::CleanUp()
{
    if (mb_Initialized)
    {
        vkDeviceWaitIdle(mDevice);

        mMainDeletionQueue.Flush();

        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

        vkDestroyDevice(mDevice, nullptr);
        vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
        vkDestroyInstance(mInstance, nullptr);

        SDL_DestroyWindow(mWnd);
    }
}

void VulkanEngine::Draw()
{
    if (SDL_GetWindowFlags(mWnd) & SDL_WINDOW_MINIMIZED) return;

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

    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearValue clearValues[2];
    float flash = abs(sin((float)mFrameIndex / 120.f));
    clearValues[0].color = { { 0.0f, 0.0f, flash, 1.0f } };
    clearValues[1].depthStencil.depth = 1.0f;

    VkRenderPassBeginInfo rpBI = VKInit::RenderPassBeginInfo(mRenderPass, mWndExtent, mFrameBuffers[swapChainImageIdx]);
    rpBI.clearValueCount = 2;
    rpBI.pClearValues = clearValues;

    vkCmdBeginRenderPass(mCmdBuffer, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

    drawObjects(mCmdBuffer, mRenderScenes.data(), mRenderScenes.size());

    vkCmdEndRenderPass(mCmdBuffer);
    // 所有操作完成，可以关闭CommandBuffer，不能写入了，可以开始执行
    VK_CHECK(vkEndCommandBuffer(mCmdBuffer));

    // 准备提交CommandBuffer到队列
    // 需要在wait信号量上等待，它表示交换链在渲染前准备完毕
    // 渲染完成后需要signal信号量
    VkSubmitInfo submit = VKInit::SubmitInfo(&mCmdBuffer);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStage;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &mPresentSem;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &mRenderSem;

    // 提交命令到队列并执行
    // renderFence会阻塞直到命令执行完成
    VK_CHECK(vkQueueSubmit(mGraphicsQueue, 1, &submit, mRenderFence));

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
            if (event.type == SDL_QUIT)
            {
                bQuit = true;
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_SPACE)
                {
                    mSelectedShader += 1;
                    if (mSelectedShader > 1) mSelectedShader = 0;
                }
            }
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

    vkb::PhysicalDeviceSelector selector{ vkbInst };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_surface(mSurface)
        .select()
        .value();

    vkb::DeviceBuilder deviceBuilder { physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    mDevice = vkbDevice.device;
    mGPU = physicalDevice.physical_device;

    mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo vmaAllocCI {};
    vmaAllocCI.physicalDevice = physicalDevice;
    vmaAllocCI.device = mDevice;
    vmaAllocCI.instance = mInstance;
    vmaCreateAllocator(&vmaAllocCI, &mAllocator);

    mMainDeletionQueue.PushFunction([&]()
    {
        vmaDestroyAllocator(mAllocator);
    });
};

void VulkanEngine::initSwapChain()
{
    vkb::SwapchainBuilder swapChainBuilder { mGPU, mDevice, mSurface };
    vkb::Swapchain vkbSwapChain = swapChainBuilder
        .use_default_format_selection()
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(mWndExtent.width, mWndExtent.height)
        .build()
        .value();

    mSwapChain = vkbSwapChain.swapchain;
    mSwapChainImages = vkbSwapChain.get_images().value();
    mSwapChainImageViews = vkbSwapChain.get_image_views().value();
    mSwapChainFormat = vkbSwapChain.image_format;

    // 删除函数，可以在CleanUp中调用Flush自动删除
    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    });

    VkExtent3D dsExtent = {
        mWndExtent.width, mWndExtent.height, 1
    };
    mDSFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo dsImageCI = VKInit::ImageCreateInfo(mDSFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, dsExtent);

    VmaAllocationCreateInfo dsAllocCI {};
    dsAllocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    dsAllocCI.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(mAllocator, &dsImageCI, &dsAllocCI, &mDSImage.mImage, &mDSImage.mAllocation, nullptr);

    VkImageViewCreateInfo dsViewCI = VKInit::ImageViewCreateInfo(mDSFormat, mDSImage.mImage, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(mDevice, &dsViewCI, nullptr, &mDSView));

    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroyImageView(mDevice, mDSView, nullptr);
        vmaDestroyImage(mAllocator, mDSImage.mImage, mDSImage.mAllocation);
    });
}

void VulkanEngine::initDefaultRenderPass()
{
    // 定义一个attachment绑定到color image上
    // attachment在renderpass开始时加载为clear，结束时保存
    // 开始状态为undefined，结束时是present状态
    VkAttachmentDescription attachDesc {};
    attachDesc.format = mSwapChainFormat;
    attachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference attachRef {};
    attachRef.attachment = 0;
    attachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription dsAttachDesc {};
    dsAttachDesc.flags = 0;
    dsAttachDesc.format = mDSFormat;
    dsAttachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    dsAttachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    dsAttachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    dsAttachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    dsAttachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    dsAttachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dsAttachDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference dsAttachRef {};
    dsAttachRef.attachment = 1;
    dsAttachRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDesc {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &attachRef;
    subpassDesc.pDepthStencilAttachment = &dsAttachRef;

    VkSubpassDependency subpassDependency {};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dsDependency {};
    dsDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dsDependency.dstSubpass = 0;
    dsDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dsDependency.srcAccessMask = 0;
    dsDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dsDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependencies[2] = { subpassDependency, dsDependency};
    VkAttachmentDescription attachments[2] = { attachDesc, dsAttachDesc };

    VkRenderPassCreateInfo rpCI {};
    rpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCI.attachmentCount = 2;
    rpCI.pAttachments = attachments;
    rpCI.subpassCount = 1;
    rpCI.pSubpasses = &subpassDesc;
    rpCI.dependencyCount = 1;
    rpCI.pDependencies = dependencies;

    VK_CHECK(vkCreateRenderPass(mDevice, &rpCI, nullptr, &mRenderPass));

    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    });
}

void VulkanEngine::initFrameBuffers()
{
    VkFramebufferCreateInfo fbCI = VKInit::FrameBufferCreateInfo(mRenderPass, mWndExtent);

    auto swapChainImageCount = static_cast<uint32_t>(mSwapChainImages.size());
    mFrameBuffers = std::vector<VkFramebuffer>(swapChainImageCount);

    for (uint32_t i = 0; i < swapChainImageCount; i++)
    {
        VkImageView attachments[2];
        attachments[0] = mSwapChainImageViews[i];
        attachments[1] = mDSView;

        fbCI.pAttachments = attachments;
        fbCI.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(mDevice, &fbCI, nullptr, &mFrameBuffers[i]));

        mMainDeletionQueue.PushFunction([=]()
        {
            vkDestroyFramebuffer(mDevice, mFrameBuffers[i], nullptr);
            vkDestroyImageView(mDevice, mSwapChainImageViews[i], nullptr);
        });
    }
}

void VulkanEngine::initCommands()
{
    VkCommandPoolCreateInfo cmdPoolCI = VKInit::CmdPoolCreateInfo(mGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(mDevice, &cmdPoolCI, nullptr, &mCmdPool));

    VkCommandBufferAllocateInfo cmdBufferAI = VKInit::CmdBufferAllocateInfo(mCmdPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdBufferAI, &mCmdBuffer));

    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroyCommandPool(mDevice, mCmdPool, nullptr);
    });
}

void VulkanEngine::initPipelines()
{
    VkShaderModule colorMeshFS;
    if (!loadShaderModule("../../Assets/Shaders/ColoredTriangle.frag.spv", &colorMeshFS))
    {
        std::cerr << "Error when building shader" << std::endl;
    }
    VkShaderModule meshVS;
    if (!loadShaderModule("../../Assets/Shaders/TriangleMesh_PushConstant.vert.spv", &meshVS))
    {
        std::cerr << "Error when building shader" << std::endl;
    }

    VkPipelineLayoutCreateInfo meshPipelineLayoutCI = VKInit::PipelineLayoutCreateInfo();
    VkPushConstantRange pushConstant;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(MeshPushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    meshPipelineLayoutCI.pushConstantRangeCount = 1;
    meshPipelineLayoutCI.pPushConstantRanges = &pushConstant;

    VkPipelineLayout meshPipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(mDevice, &meshPipelineLayoutCI, nullptr, &meshPipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.mShaderStageCIs.push_back(
        VKInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, meshVS));
    pipelineBuilder.mShaderStageCIs.push_back(
        VKInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, colorMeshFS));

    pipelineBuilder.mPipelineLayout = meshPipelineLayout;

    pipelineBuilder.mVIState = VKInit::PipelineVIStateCreateInfo();
    pipelineBuilder.mIAState = VKInit::PipelineIAStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pipelineBuilder.mViewport.x = 0.0f;
    pipelineBuilder.mViewport.y = 0.0f;
    pipelineBuilder.mViewport.width = (float)mWndExtent.width;
    pipelineBuilder.mViewport.height = (float)mWndExtent.height;
    pipelineBuilder.mViewport.minDepth = 0.0f;
    pipelineBuilder.mViewport.maxDepth = 1.0f;
    pipelineBuilder.mScissor.offset = {0, 0};
    pipelineBuilder.mScissor.extent = mWndExtent;

    pipelineBuilder.mRSState = VKInit::PipelineRSStateCreateInfo(VK_POLYGON_MODE_FILL);
    pipelineBuilder.mMSState = VKInit::PipelineMSStateCreateInfo();
    pipelineBuilder.mCBAttach = VKInit::PipelineCBAttachState();
    pipelineBuilder.mDSState = VKInit::PipelineDSStateCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    VertexInputDesc viDesc = Vertex::GetVertexDesc();
    pipelineBuilder.mVIState.pVertexAttributeDescriptions = viDesc.mAttributes.data();
    pipelineBuilder.mVIState.vertexAttributeDescriptionCount = viDesc.mAttributes.size();
    pipelineBuilder.mVIState.pVertexBindingDescriptions = viDesc.mBindings.data();
    pipelineBuilder.mVIState.vertexBindingDescriptionCount = viDesc.mBindings.size();

    VkPipeline meshPipeline = pipelineBuilder.BuildPipeline(mDevice, mRenderPass);

    createMaterial(meshPipeline, meshPipelineLayout, "DefaultMesh");

    vkDestroyShaderModule(mDevice, meshVS, nullptr);
    vkDestroyShaderModule(mDevice, colorMeshFS, nullptr);

    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroyPipeline(mDevice, meshPipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, meshPipelineLayout, nullptr);
    });
}

void VulkanEngine::initScene()
{
    RenderScene scene{};
    scene.mMesh = getMesh("ObjMesh");
    scene.mMaterial = getMaterial("DefaultMesh");
    scene.mTransformMatrix = glm::mat4 {1.0f};

    mRenderScenes.push_back(scene);

    for (int x = -20; x <= 20; x++)
    {
        for (int y = -20; y <= 20; y++)
        {
            RenderScene tri{};
            tri.mMesh = getMesh("Triangle");
            tri.mMaterial = getMaterial("DefaultMesh");
            glm::mat4 translation = glm::translate(glm::mat4{1.0f}, glm::vec3(x, 0, y));
            glm::mat4 scale = glm::scale(glm::mat4{1.0f}, glm::vec3(0.2, 0.2, 0.2));
            tri.mTransformMatrix = translation * scale;

            mRenderScenes.push_back(tri);
        }
    }
}

void VulkanEngine::initSyncObjects()
{
    VkFenceCreateInfo fenceCI = VKInit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(mDevice, &fenceCI, nullptr, &mRenderFence));

    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroyFence(mDevice, mRenderFence, nullptr);
    });

    VkSemaphoreCreateInfo semCI = VKInit::SemaphoreCreateInfo();
    VK_CHECK(vkCreateSemaphore(mDevice, &semCI, nullptr, &mPresentSem));
    VK_CHECK(vkCreateSemaphore(mDevice, &semCI, nullptr, &mRenderSem));

    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroySemaphore(mDevice, mPresentSem, nullptr);
        vkDestroySemaphore(mDevice, mRenderSem, nullptr);
    });
}

bool VulkanEngine::loadShaderModule(const char *filepath, VkShaderModule *outShaderModule)
{
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    size_t fileSize = (size_t)file.tellg();

    // Spirv需要一个uint32_t的Buffer，所以要确保空间足够
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // 将指针移动起始位置
    file.seekg(0);

    // 把文件加载到Buffer
    file.read((char*)buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo smCI = {};
    smCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smCI.pNext = nullptr;
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

void VulkanEngine::loadMeshes()
{
    Mesh triMesh{}, objMesh{};
    //make the array 3 vertices long
    triMesh.mVertices.resize(3);

    //vertex positions
    triMesh.mVertices[0].mPosition = { 1.f,1.f, 0.0f };
    triMesh.mVertices[1].mPosition = { -1.f,1.f, 0.0f };
    triMesh.mVertices[2].mPosition = { 0.f,-1.f, 0.0f };
    //vertex colors, all green
    triMesh.mVertices[0].mColor = { 0.f,1.f, 0.0f }; //pure green
    triMesh.mVertices[1].mColor = { 0.f,1.f, 0.0f }; //pure green
    triMesh.mVertices[2].mColor = { 0.f,1.f, 0.0f }; //pure green

    //load the monkey
    objMesh.LoadFromOBJ("../../Assets/Models/monkey_smooth.obj");

    uploadMesh(triMesh);
    uploadMesh(objMesh);

    mMeshes["ObjMesh"] = objMesh;
    mMeshes["Triangle"] = triMesh;
}

void VulkanEngine::uploadMesh(Mesh& mesh)
{
    //allocate vertex buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    //this is the total size, in bytes, of the buffer we are allocating
    bufferInfo.size = mesh.mVertices.size() * sizeof(Vertex);
    //this buffer is going to be used as a Vertex Buffer
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    //let the VMA library know that this data should be writeable by CPU, but also readable by GPU
    VmaAllocationCreateInfo vmaAllocCI = {};
    vmaAllocCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(
        mAllocator, &bufferInfo, &vmaAllocCI,
        &mesh.mVertexBuffer.mBuffer,
        &mesh.mVertexBuffer.mAllocation,
        nullptr));

    //add the destruction of triangle mesh buffer to the deletion queue
    mMainDeletionQueue.PushFunction([=]()
    {
        vmaDestroyBuffer(mAllocator, mesh.mVertexBuffer.mBuffer, mesh.mVertexBuffer.mAllocation);
    });

    //copy vertex data
    void* data;
    vmaMapMemory(mAllocator, mesh.mVertexBuffer.mAllocation, &data);

    memcpy(data, mesh.mVertices.data(), mesh.mVertices.size() * sizeof(Vertex));

    vmaUnmapMemory(mAllocator, mesh.mVertexBuffer.mAllocation);
}

Material* VulkanEngine::createMaterial(VkPipeline pipeline, VkPipelineLayout pipelineLayout, const std::string& name)
{
    Material mat{};
    mat.mPipeline = pipeline;
    mat.mPipelineLayout = pipelineLayout;
    mMaterials[name] = mat;

    return &mMaterials[name];
}

Material* VulkanEngine::getMaterial(const std::string& name)
{
    auto it = mMaterials.find(name);
    if (it == mMaterials.end()) return nullptr;
    else return &(*it).second;
}

Mesh* VulkanEngine::getMesh(const std::string& name)
{
    auto it = mMeshes.find(name);
    if (it == mMeshes.end()) return nullptr;
    else return &(*it).second;
}

void VulkanEngine::drawObjects(VkCommandBuffer cmdBuffer, RenderScene* first, uint32_t count)
{
    glm::vec3 camPos = {0.0f, -2.0f, -10.0f};

    glm::mat4 view = glm::translate(glm::mat4(1.0f), camPos);
    glm::mat4 proj = glm::perspective(glm::radians(70.f), (float)mWndExtent.width / (float)mWndExtent.height, 0.1f, 200.0f);
    proj[1][1] *= -1;

    Mesh* lastMesh = nullptr;
    Material* lastMat = nullptr;
    for (int i = 0; i < count; i++)
    {
        RenderScene& scene = first[i];

        if (scene.mMaterial != lastMat)
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scene.mMaterial->mPipeline);
            lastMat = scene.mMaterial;
        }

        glm::mat4 model = scene.mTransformMatrix;
        //final render matrix, that we are calculating on the cpu
        glm::mat4 meshMatrix = proj * view * model;

        MeshPushConstants constants{};
        constants.mMat = meshMatrix;

        vkCmdPushConstants(cmdBuffer, scene.mMaterial->mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

        if (scene.mMesh != lastMesh)
        {
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &scene.mMesh->mVertexBuffer.mBuffer, &offset);
            lastMesh = scene.mMesh;
        }
        vkCmdDraw(cmdBuffer, scene.mMesh->mVertices.size(), 1, 0, 0);
    }
}