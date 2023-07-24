#include "VKRendererBase.hpp"

std::vector<const char*> VKRendererBase::mArgs;

// Public
VKRendererBase::VKRendererBase(bool msaa, bool enableValidation)
{
    // Check for a valid asset path
    struct stat info{};
    if (stat(GetAssetsPath().c_str(), &info) != 0)
    {
        std::string msg = "Could not locate asset path in \"" + GetAssetsPath() + "\" !";
        MessageBox(nullptr, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
        exit(-1);
    }

    mSettings.multiSampling = msaa;
    mSettings.sampleCount = msaa ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
    mSettings.validation = enableValidation;
    mUIOverlay.mMSAA = mSettings.sampleCount;

    // Command line arguments
    mCmdLineParser.Add("help", { "--help" }, 0, "Show help");
    mCmdLineParser.Add("validation", { "-v", "--validation" }, 0, "Enable validation layers");
    mCmdLineParser.Add("vsync", { "-vs", "--vsync" }, 0, "Enable V-Sync");
    mCmdLineParser.Add("fullscreen", { "-f", "--fullscreen" }, 0, "Start in fullscreen mode");
    mCmdLineParser.Add("width", { "-w", "--width" }, 1, "Set window width");
    mCmdLineParser.Add("height", { "-h", "--height" }, 1, "Set window height");
    mCmdLineParser.Add("shaders", { "-s", "--shaders" }, 1, "Select shader type to use (glsl or hlsl)");
    mCmdLineParser.Add("gpuselection", { "-g", "--gpu" }, 1, "Select GPU to run on");
    mCmdLineParser.Add("gpulist", { "-gl", "--listgpus" }, 0, "Display a list of available Vulkan devices");
    mCmdLineParser.Add("benchmark", { "-b", "--benchmark" }, 0, "Run example in benchmark mode");
    mCmdLineParser.Add("benchmarkwarmup", { "-bw", "--benchwarmup" }, 1, "Set warmup time for benchmark mode in seconds");
    mCmdLineParser.Add("benchmarkruntime", { "-br", "--benchruntime" }, 1, "Set duration time for benchmark mode in seconds");
    mCmdLineParser.Add("benchmarkresultfile", { "-bf", "--benchfilename" }, 1, "Set file name for benchmark results");
    mCmdLineParser.Add("benchmarkresultframes", { "-bt", "--benchframetimes" }, 0, "Save frame times to benchmark results file");
    mCmdLineParser.Add("benchmarkframes", { "-bfs", "--benchmarkframes" }, 1, "Only render the given number of frames");

    mCmdLineParser.Parse(mArgs);
    if (mCmdLineParser.IsSet("help")) 
    {

        SetupConsole("Debug Console");
        mCmdLineParser.PrintHelp();
        std::cin.get();
        exit(0);
    }
    if (mCmdLineParser.IsSet("validation")) mSettings.validation = true;
    
    if (mCmdLineParser.IsSet("vsync")) mSettings.vsync = true;

    if (mCmdLineParser.IsSet("height")) mHeight = mCmdLineParser.GetValueAsInt("height", (int)mHeight);

    if (mCmdLineParser.IsSet("width")) mWidth = mCmdLineParser.GetValueAsInt("width", (int)mWidth);

    if (mCmdLineParser.IsSet("fullscreen")) mSettings.fullscreen = true;

    if (mCmdLineParser.IsSet("shaders"))
    {
        std::string value = mCmdLineParser.GetValueAsString("shaders", "glsl");

        if ((value != "glsl") && (value != "hlsl")) std::cerr << "Shader type must be one of 'glsl' or 'hlsl'\n";
        else mShaderDir = value;
    }

    if (mCmdLineParser.IsSet("benchmark"))
    {
        mBenchmark.mbActive = true;
        LeoVK::VKTools::bErrorModeSilent = true;
    }

    if (mCmdLineParser.IsSet("benchmarkwarmup")) {
        mBenchmark.mWarmup = mCmdLineParser.GetValueAsInt("benchmarkwarmup", mBenchmark.mWarmup);
    }
    if (mCmdLineParser.IsSet("benchmarkruntime")) {
        mBenchmark.mDuration = mCmdLineParser.GetValueAsInt("benchmarkruntime", mBenchmark.mDuration);
    }
    if (mCmdLineParser.IsSet("benchmarkresultfile")) {
        mBenchmark.mFilename = mCmdLineParser.GetValueAsString("benchmarkresultfile", mBenchmark.mFilename);
    }
    if (mCmdLineParser.IsSet("benchmarkresultframes")) {
        mBenchmark.mbOutputFrameTime = true;
    }
    if (mCmdLineParser.IsSet("benchmarkframes")) {
        mBenchmark.mOutputFrames = mCmdLineParser.GetValueAsInt("benchmarkframes", mBenchmark.mOutputFrames);
    }

    // Enable console if validation is active, debug message callback will output to it
    if (this->mSettings.validation)
    {
        SetupConsole("Debug Console");
    }
    SetupDPIAwareness();
}

VKRendererBase::~VKRendererBase()
{
    // Clean up Vulkan resources
    mSwapChain.Cleanup();
    if (mDescPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
    }
    destroyCommandBuffers();
    if (mRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    }
    for (auto & mFrameBuffer : mFrameBuffers)
    {
        vkDestroyFramebuffer(mDevice, mFrameBuffer, nullptr);
    }

    for (auto& shaderModule : mShaderModules)
    {
        vkDestroyShaderModule(mDevice, shaderModule, nullptr);
    }
    vkDestroyImageView(mDevice, mDepthStencil.imageView, nullptr);
    vkDestroyImage(mDevice, mDepthStencil.image, nullptr);
    vkFreeMemory(mDevice, mDepthStencil.memory, nullptr);

    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
    vkDestroyCommandPool(mDevice, mCmdPool, nullptr);

    if (mSettings.multiSampling)
    {
        vkDestroyImage(mDevice, mMSTarget.color.image, nullptr);
        vkDestroyImageView(mDevice, mMSTarget.color.imageView, nullptr);
        vkFreeMemory(mDevice, mMSTarget.color.memory, nullptr);
        vkDestroyImage(mDevice, mMSTarget.depth.image, nullptr);
        vkDestroyImageView(mDevice, mMSTarget.depth.imageView, nullptr);
        vkFreeMemory(mDevice, mMSTarget.depth.memory, nullptr);
    }

    vkDestroySemaphore(mDevice, mSemaphores.presentComplete, nullptr);
    vkDestroySemaphore(mDevice, mSemaphores.renderComplete, nullptr);
    for (auto& fence : mWaitFences)
    {
        vkDestroyFence(mDevice, fence, nullptr);
    }

    if (mSettings.overlay) {
        mUIOverlay.FreeResources();
    }

    delete mpVulkanDevice;

    if (mSettings.validation)
    {
        LeoVK::Debug::FreeDebugCallback(mInstance);
    }
    vkDestroyInstance(mInstance, nullptr);
}

bool VKRendererBase::InitVulkan()
{
    return false;
}

void VKRendererBase::SetupConsole(std::string title)
{

}

void VKRendererBase::SetupDPIAwareness()
{

}

HWND VKRendererBase::SetupWindow(HINSTANCE hInstance, WNDPROC wndProc)
{
    return nullptr;
}

void VKRendererBase::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

}

VkResult VKRendererBase::CreateInstance(bool enableValidation)
{
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void VKRendererBase::ViewChanged()
{

}

void VKRendererBase::KeyPressed(uint32_t)
{

}

void VKRendererBase::MouseMoved(double x, double y, bool &handled)
{

}

void VKRendererBase::WindowResized()
{

}

void VKRendererBase::BuildCommandBuffers()
{

}

void VKRendererBase::SetupDepthStencil()
{

}

void VKRendererBase::SetupFrameBuffer()
{

}

void VKRendererBase::SetupRenderPass()
{

}

void VKRendererBase::GetEnabledFeatures()
{

}

void VKRendererBase::GetEnabledExtensions()
{

}

void VKRendererBase::Prepare()
{

}

VkPipelineShaderStageCreateInfo VKRendererBase::LoadShader(std::string fileName, VkShaderStageFlagBits stage)
{
    return VkPipelineShaderStageCreateInfo();
}

void VKRendererBase::RenderLoop()
{

}

void VKRendererBase::DrawUI(VkCommandBuffer commandBuffer)
{

}

void VKRendererBase::PrepareFrame()
{

}

void VKRendererBase::SubmitFrame()
{

}

void VKRendererBase::RenderFrame()
{

}

void VKRendererBase::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{

}

void VKRendererBase::FileDropped(std::string &filename)
{

}

void VKRendererBase::OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

}

// Protected
std::string VKRendererBase::GetShadersPath() const
{
    return std::string();
}

// Private
std::string VKRendererBase::getWindowTitle()
{
    return std::string();
}

void VKRendererBase::windowResize()
{

}

void VKRendererBase::handleMouseMove(int32_t x, int32_t y)
{

}

void VKRendererBase::nextFrame()
{

}

void VKRendererBase::updateOverlay()
{

}

void VKRendererBase::createPipelineCache()
{

}

void VKRendererBase::createCommandPool()
{

}

void VKRendererBase::createSynchronizationPrimitives()
{

}

void VKRendererBase::initSwapChain()
{

}

void VKRendererBase::setupSwapChain()
{

}

void VKRendererBase::createCommandBuffers()
{

}

void VKRendererBase::destroyCommandBuffers()
{

}

void VKRendererBase::setupRenderTarget(VkImage *image, VkImageView *imageView, VkDeviceMemory *memory, bool isDepth)
{

}
