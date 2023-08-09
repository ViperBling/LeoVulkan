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
    VkClearValue clearValue;
    float flash = abs(sin((float)mFrameIndex / 120.f));
    clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

    VkRenderPassBeginInfo rpBI = VKInit::RenderPassBeginInfo(mRenderPass, mWndExtent, mFrameBuffers[swapChainImageIdx]);
    rpBI.clearValueCount = 1;
    rpBI.pClearValues = &clearValue;

    vkCmdBeginRenderPass(mCmdBuffer, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

    if (mSelectedShader == 0)
    {
        vkCmdBindPipeline(mCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mTrianglePipeline);
    }
    else
    {
        vkCmdBindPipeline(mCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mRedTrianglePipeline);
    }
    vkCmdDraw(mCmdBuffer, 3, 1, 0, 0);

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
        .require_api_version(1, 1, 0)
        .build();

    vkb::Instance vkbInst = inst.value();

    mInstance = vkbInst.instance;
    mDebugMessenger = vkbInst.debug_messenger;

    if (!SDL_Vulkan_CreateSurface(mWnd, mInstance, &mSurface))
    {
        std::cerr << "Fail to create surface" << std::endl;
    }

    vkb::PhysicalDeviceSelector selector{ vkbInst };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 1)
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
    vkb::SwapchainBuilder swapChainBuilder { mGPU, mDevice, mSurface };
    vkb::Swapchain vkbSwapChain = swapChainBuilder
        .use_default_format_selection()
        .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
        .set_desired_extent(mWndExtent.width, mWndExtent.height)
        .build()
        .value();

    mSwapChain = vkbSwapChain.swapchain;
    mSwapChainImages = vkbSwapChain.get_images().value();
    mSwapChainImageViews = vkbSwapChain.get_image_views().value();
    mSwapChainFormat = vkbSwapChain.image_format;

    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    });
}

void VulkanEngine::initDefaultRenderPass()
{
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

    VkSubpassDescription subpassDesc {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &attachRef;

    VkSubpassDependency subpassDependency {};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpCI {};
    rpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCI.attachmentCount = 1;
    rpCI.pAttachments = &attachDesc;
    rpCI.subpassCount = 1;
    rpCI.pSubpasses = &subpassDesc;
    rpCI.dependencyCount = 1;
    rpCI.pDependencies = &subpassDependency;

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
        fbCI.pAttachments = &mSwapChainImageViews[i];
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
    VkShaderModule triangleFS;
    if (!loadShaderModule("../../Assets/Shaders/ColoredTriangle.frag.spv", &triangleFS))
    {
        std::cerr << "Error when building shader" << std::endl;
    }
    VkShaderModule triangleVS;
    if (!loadShaderModule("../../Assets/Shaders/ColoredTriangle.vert.spv", &triangleVS))
    {
        std::cerr << "Error when building shader" << std::endl;
    }
    VkShaderModule redTriangleFS;
    if (!loadShaderModule("../../Assets/Shaders/Triangle.frag.spv", &redTriangleFS))
    {
        std::cerr << "Error when building shader" << std::endl;
    }
    VkShaderModule redTriangleVS;
    if (!loadShaderModule("../../Assets/Shaders/Triangle.vert.spv", &redTriangleVS))
    {
        std::cerr << "Error when building shader" << std::endl;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCI = VKInit::PipelineLayoutCreateInfo();
    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutCI, nullptr, &mTrianglePipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.mShaderStageCIs.push_back(
        VKInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, triangleVS));
    pipelineBuilder.mShaderStageCIs.push_back(
        VKInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFS));

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
    pipelineBuilder.mPipelineLayout = mTrianglePipelineLayout;

    mTrianglePipeline = pipelineBuilder.BuildPipeline(mDevice, mRenderPass);

    pipelineBuilder.mShaderStageCIs.clear();
    pipelineBuilder.mShaderStageCIs.push_back(
        VKInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, redTriangleVS));
    pipelineBuilder.mShaderStageCIs.push_back(
        VKInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, redTriangleFS));

    mRedTrianglePipeline = pipelineBuilder.BuildPipeline(mDevice, mRenderPass);

    vkDestroyShaderModule(mDevice, redTriangleVS, nullptr);
    vkDestroyShaderModule(mDevice, redTriangleFS, nullptr);
    vkDestroyShaderModule(mDevice, triangleVS, nullptr);
    vkDestroyShaderModule(mDevice, triangleFS, nullptr);

    mMainDeletionQueue.PushFunction([=]()
    {
        vkDestroyPipeline(mDevice, mRedTrianglePipeline, nullptr);
        vkDestroyPipeline(mDevice, mTrianglePipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, mTrianglePipelineLayout, nullptr);
    });
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


