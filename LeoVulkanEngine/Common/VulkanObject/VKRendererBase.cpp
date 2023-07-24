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
    mCmdLineParser.Add("gpuSelection", { "-g", "--gpu" }, 1, "Select GPU to run on");
    mCmdLineParser.Add("gpuList", { "-gl", "--listGPUs" }, 0, "Display a list of available Vulkan devices");
    mCmdLineParser.Add("benchmark", { "-b", "--benchmark" }, 0, "Run example in benchmark mode");
    mCmdLineParser.Add("benchmarkWarmup", { "-bw", "--benchWarmup" }, 1, "Set warmup time for benchmark mode in seconds");
    mCmdLineParser.Add("benchmarkRuntime", { "-br", "--benchRuntime" }, 1, "Set duration time for benchmark mode in seconds");
    mCmdLineParser.Add("benchmarkResultFile", { "-bf", "--benchFilename" }, 1, "Set file name for benchmark results");
    mCmdLineParser.Add("benchmarkResultFrames", { "-bt", "--benchFrameTimes" }, 0, "Save frame times to benchmark results file");
    mCmdLineParser.Add("benchmarkFrames", { "-bfs", "--benchmarkFrames" }, 1, "Only render the given number of frames");

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
    VkResult result;

    // Instance
    result = CreateInstance(mSettings.validation);
    if (result)
    {
        LeoVK::VKTools::ExitFatal("Could not create Vulkan instance : \n" + LeoVK::VKTools::ErrorString(result), result);
        return false;
    }
    if (mSettings.validation) LeoVK::Debug::SetupDebugging(mInstance);

    // Physical Device
    uint32_t gpuCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr))
    if (gpuCount == 0)
    {
        LeoVK::VKTools::ExitFatal("No suitable vulkan device! ", -1);
        return false;
    }
    // Enumerate Device
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    result = vkEnumeratePhysicalDevices(mInstance, &gpuCount, physicalDevices.data());
    if (result)
    {
        LeoVK::VKTools::ExitFatal("Could not enumerate physical devices : \n" + LeoVK::VKTools::ErrorString(result), result);
        return false;
    }

    // Select GPU
    uint32_t selectedDevice = 0;
    if (mCmdLineParser.IsSet("gpuSelection"))
    {
        uint32_t index = mCmdLineParser.GetValueAsInt("gpuSelection", 0);
        if (index > gpuCount - 1)
        {
            std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listGPUs to show available Vulkan devices)" << "\n";
        }
        else
        {
            selectedDevice = index;
        }
    }
    if (mCmdLineParser.IsSet("gpuList"))
    {
        std::cout << "Available Vulkan Device" << "\n";
        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDeviceProperties deviceProps;
            vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
            std::cout << "Device [" << i << "] : " << deviceProps.deviceName << std::endl;
            std::cout << " Type: " << LeoVK::VKTools::PhysicalDeviceTypeString(deviceProps.deviceType) << "\n";
            std::cout << " API: " << (deviceProps.apiVersion >> 22) << "." << ((deviceProps.apiVersion >> 12) & 0x3ff) << "." << (deviceProps.apiVersion & 0xfff) << "\n";
        }
    }

    mPhysicalDevice = physicalDevices[selectedDevice];

    // Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mDeviceProps);
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mDeviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mDeviceMemoryProps);

    // 子类可重写此方法以设置一些必要的Feature
    GetEnabledFeatures();

    // Vulkan device creation
    // This is handled by a separate class that gets a logical device representation
    // and encapsulates functions related to a device
    mpVulkanDevice = new LeoVK::VulkanDevice(mPhysicalDevice);

    // 子类可以启用从物理设备中获取的一系列扩展
    GetEnabledExtensions();

    result = mpVulkanDevice->CreateLogicalDevice(mEnabledFeatures, mEnabledDeviceExtensions, mpDeviceCreatepNexChain);
    if (result != VK_SUCCESS)
    {
        LeoVK::VKTools::ExitFatal("Could not create Vulkan device: \n" + LeoVK::VKTools::ErrorString(result), result);
        return false;
    }

    mDevice = mpVulkanDevice->mLogicalDevice;

    vkGetDeviceQueue(mDevice, mpVulkanDevice->mQueueFamilyIndices.graphics, 0, &mQueue);

    // Find a suitable Depth format
    VkBool32 validDepthFormat = LeoVK::VKTools::GetSupportedDepthFormat(mPhysicalDevice, &mDepthFormat);
    assert(validDepthFormat);

    mSwapChain.Connect(mInstance, mPhysicalDevice, mDevice);

    // Sync Objects
    VkSemaphoreCreateInfo semaphoreCI = LeoVK::Init::SemaphoreCreateInfo();
    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCI, nullptr, &mSemaphores.presentComplete))
    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCI, nullptr, &mSemaphores.renderComplete))

    // Set up submit info struct
    mSubmitInfo = LeoVK::Init::SubmitInfo();
    mSubmitInfo.pWaitDstStageMask = &mSubmitPipelineStages;
    mSubmitInfo.waitSemaphoreCount = 1;
    mSubmitInfo.pWaitSemaphores = &mSemaphores.presentComplete;
    mSubmitInfo.signalSemaphoreCount = 1;
    mSubmitInfo.pSignalSemaphores = &mSemaphores.renderComplete;

    return true;
}

void VKRendererBase::SetupConsole(std::string title)
{
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* stream;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    freopen_s(&stream, "CONOUT$", "w+", stderr);
    SetConsoleTitle(TEXT(title.c_str()));
}

void VKRendererBase::SetupDPIAwareness()
{
    typedef HRESULT *(__stdcall *SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);

    HMODULE shCore = LoadLibraryA("Shcore.dll");
    if (shCore)
    {
        SetProcessDpiAwarenessFunc setProcessDpiAwareness =
            (SetProcessDpiAwarenessFunc)GetProcAddress(shCore, "SetProcessDpiAwareness");

        if (setProcessDpiAwareness != nullptr)
        {
            setProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }

        FreeLibrary(shCore);
    }
}

HWND VKRendererBase::SetupWindow(HINSTANCE hInstance, WNDPROC wndProc)
{
    this->mHInstance = hInstance;

    WNDCLASSEX wndClass;

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = wndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = mName.c_str();
    wndClass.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

    if (!RegisterClassEx(&wndClass))
    {
        std::cout << "Could not register window class!\n";
        fflush(stdout);
        exit(1);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (mSettings.fullscreen)
    {
        if ((mWidth != (uint32_t)screenWidth) && (mHeight != (uint32_t)screenHeight))
        {
            DEVMODE dmScreenSettings;
            memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
            dmScreenSettings.dmSize       = sizeof(dmScreenSettings);
            dmScreenSettings.dmPelsWidth  = mWidth;
            dmScreenSettings.dmPelsHeight = mHeight;
            dmScreenSettings.dmBitsPerPel = 32;
            dmScreenSettings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
            {
                if (MessageBox(nullptr, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
                {
                    mSettings.fullscreen = false;
                }
                else
                {
                    return nullptr;
                }
            }
            screenWidth = (int)mWidth;
            screenHeight = (int)mHeight;
        }
    }

    DWORD dwExStyle;
    DWORD dwStyle;

    if (mSettings.fullscreen)
    {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }
    else
    {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }

    RECT windowRect;
    windowRect.left = 0L;
    windowRect.top = 0L;
    windowRect.right = mSettings.fullscreen ? (long)screenWidth : (long)mWidth;
    windowRect.bottom = mSettings.fullscreen ? (long)screenHeight : (long)mHeight;

    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

    std::string windowTitle = getWindowTitle();
    mHwnd = CreateWindowEx(
        0,
        mName.c_str(),
        windowTitle.c_str(),
        dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!mSettings.fullscreen)
    {
        // Center on screen
        uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
        uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
        SetWindowPos(mHwnd, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    if (!mHwnd)
    {
        printf("Could not create window!\n");
        fflush(stdout);
        return nullptr;
    }

    ShowWindow(mHwnd, SW_SHOW);
    SetForegroundWindow(mHwnd);
    SetFocus(mHwnd);

    return mHwnd;
}

void VKRendererBase::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CLOSE:
            mbPrepared = false;
            DestroyWindow(hWnd);
            PostQuitMessage(0);
            break;
        case WM_PAINT:
            ValidateRect(mHwnd, nullptr);
            break;
        case WM_KEYDOWN:
            switch (wParam)
            {
                case KEY_P:
                    mbPaused = !mbPaused;
                    break;
                case KEY_F1:
                    mUIOverlay.mbVisible = !mUIOverlay.mbVisible;
                    mUIOverlay.mbUpdated = true;
                    break;
                case KEY_ESCAPE:
                    PostQuitMessage(0);
                    break;
                default:
                    break;
            }

            if (mCamera.mType == FirstPerson)
            {
                switch (wParam)
                {
                    case KEY_W:
                        mCamera.mKeys.mUp = true;
                        break;
                    case KEY_S:
                        mCamera.mKeys.mDown = true;
                        break;
                    case KEY_A:
                        mCamera.mKeys.mLeft = true;
                        break;
                    case KEY_D:
                        mCamera.mKeys.mRight = true;
                        break;
                    default:
                        break;
                }
            }

            KeyPressed((uint32_t)wParam);
            break;
        case WM_KEYUP:
            if (mCamera.mType == FirstPerson)
            {
                switch (wParam)
                {
                    case KEY_W:
                        mCamera.mKeys.mUp = false;
                        break;
                    case KEY_S:
                        mCamera.mKeys.mDown = false;
                        break;
                    case KEY_A:
                        mCamera.mKeys.mLeft = false;
                        break;
                    case KEY_D:
                        mCamera.mKeys.mRight = false;
                        break;
                    default:
                        break;
                }
            }
            break;
        case WM_LBUTTONDOWN:
            mMousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
            mMouseButtons.left = true;
            break;
        case WM_RBUTTONDOWN:
            mMousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
            mMouseButtons.right = true;
            break;
        case WM_MBUTTONDOWN:
            mMousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
            mMouseButtons.middle = true;
            break;
        case WM_LBUTTONUP:
            mMouseButtons.left = false;
            break;
        case WM_RBUTTONUP:
            mMouseButtons.right = false;
            break;
        case WM_MBUTTONUP:
            mMouseButtons.middle = false;
            break;
        case WM_MOUSEWHEEL:
        {
            short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            mCamera.Translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
            mbViewUpdated = true;
            break;
        }
        case WM_MOUSEMOVE:
        {
            handleMouseMove(LOWORD(lParam), HIWORD(lParam));
            break;
        }
        case WM_SIZE:
            if ((mbPrepared) && (wParam != SIZE_MINIMIZED))
            {
                if ((mbResizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
                {
                    mDstWidth = LOWORD(lParam);
                    mDstHeight = HIWORD(lParam);
                    windowResize();
                }
            }
            break;
        case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
            minMaxInfo->ptMinTrackSize.x = 64;
            minMaxInfo->ptMinTrackSize.y = 64;
            break;
        }
        case WM_ENTERSIZEMOVE:
            mbResizing = true;
            break;
        case WM_EXITSIZEMOVE:
            mbResizing = false;
            break;
        case WM_DROPFILES:
        {
            std::string fname;
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            // extract files here
            char filename[MAX_PATH];
            uint32_t count = DragQueryFileA(hDrop, -1, nullptr, 0);
            for (uint32_t i = 0; i < count; ++i)
            {
                if (DragQueryFileA(hDrop, i, filename, MAX_PATH))
                {
                    fname = filename;
                }
                break;
            }
            DragFinish(hDrop);
            FileDropped(fname);
            break;
        }
    }

    OnHandleMessage(hWnd, uMsg, wParam, lParam);
}

VkResult VKRendererBase::CreateInstance(bool enableValidation)
{
    mSettings.validation = enableValidation;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = mName.c_str();
    appInfo.pEngineName = mName.c_str();
    appInfo.apiVersion = mAPIVersion;

    std::vector<const char*> instExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    instExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

    // Get extensions supported by the instance and store for later use
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (extCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
        {
            for (VkExtensionProperties extension : extensions) mSupportedInstanceExtensions.emplace_back(extension.extensionName);
        }
    }

    // Enabled requested instance extensions
    if (!mEnabledInstanceExtensions.empty())
    {
        for (const char * enabledExtension : mEnabledInstanceExtensions)
        {
            // Output message if requested extension is not available
            if (std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), enabledExtension) == mSupportedInstanceExtensions.end())
            {
                std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
            }
            instExtensions.push_back(enabledExtension);
        }
    }
    VkInstanceCreateInfo instanceCI {};
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pNext = nullptr;
    instanceCI.pApplicationInfo = &appInfo;

    if (!instExtensions.empty())
    {
        if (mSettings.validation)
        {
            instExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// SRS - Dependency when VK_EXT_DEBUG_MARKER is enabled
            instExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        instanceCI.enabledExtensionCount = (uint32_t)instExtensions.size();
        instanceCI.ppEnabledExtensionNames = instExtensions.data();
    }

    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
    // Note that on Android this layer requires at least NDK r20
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    if (mSettings.validation)
    {
        // Check if this layer is available at instance level
        uint32_t instanceLayerCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
        bool validationLayerPresent = false;
        for (VkLayerProperties layer : instanceLayerProperties)
        {
            if (strcmp(layer.layerName, validationLayerName) == 0)
            {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent) {
            instanceCI.ppEnabledLayerNames = &validationLayerName;
            instanceCI.enabledLayerCount = 1;
        } else {
            std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
        }
    }
    return vkCreateInstance(&instanceCI, nullptr, &mInstance);
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
