﻿#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <limits>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <map>
#include <optional>
#include <set>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>
#include <complex>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAME_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG   // No Debug
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) func(instance, debugMessenger, pAllocator);
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescription()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescription{};
        attributeDescription[0].binding = 0;
        attributeDescription[0].location = 0;
        attributeDescription[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescription[0].offset = offsetof(Vertex, pos);

        attributeDescription[1].binding = 0;
        attributeDescription[1].location = 1;
        attributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescription[1].offset = offsetof(Vertex, color);

        attributeDescription[2].binding = 0;
        attributeDescription[2].location = 2;
        attributeDescription[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescription[2].offset = offsetof(Vertex, texCoord);

        return attributeDescription;
    }
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class HelloTriangleApplication
{
public:
    void Run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(logicalDevice);
    }

    void initWindow()
    {
        glfwInit();
        // 设置窗口属性
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // 创建窗口
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);
    }

    static void frameBufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->frameBufferResized = true;
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFrameBuffers();
        createCommandPool();
        
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        
        createVertexBuffer();
        createIndexBuffer();
        
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffer();
        createSyncObjects();
    }

    void cleanup()
    {
        cleanupSwapChain();

        vkDestroySampler(logicalDevice, textureSampler, nullptr);
        vkDestroyImageView(logicalDevice, textureImageView, nullptr);
        vkDestroyImage(logicalDevice, textureImage, nullptr);
        vkFreeMemory(logicalDevice, textureImageMemory, nullptr);
        
        vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
            vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
        }

        vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

        vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
        vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);

        vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
        vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
        vkDestroyDevice(logicalDevice, nullptr);

        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance()
    {
        if (enableValidationLayers && !checkingValidationLayerSupport())
        {
            throw std::runtime_error("Validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engines";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        // uint32_t glfwExtensionCount = 0;
        // const char** glfwExtensions;
        // glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // std::vector<VkExtensionProperties> extensions(glfwExtensionCount);
        // vkEnumerateInstanceExtensionProperties(nullptr, &glfwExtensionCount, extensions.data());
        // std::cout << "Avaliable extensions: \n";
        // for (const auto & extension : extensions)
        // {
        //     std::cout << '\t' << extension.extensionName << '\n';
        // }

        auto extensions = getRequiredExtensions();

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create instance!");
        }
    }

    // ======================================= Validation Layers Begin ======================================= //
    bool checkingValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers)
        {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers)
            {
                // std::cout << layerProperties.layerName << std::endl;
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) return false;
        }
        return true;
    }

    std::vector<const char*> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }
    // ======================================= Validation Layers End ======================================= //

    // ======================================= Debug Begin ======================================= //
    void setupDebugMessenger()
    {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to set up debug messenger!");
        }
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |      // 日志等级
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |              // 启用全部日志类型
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
        VkDebugUtilsMessageTypeFlagsEXT msgType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
    // ======================================= Debug End ======================================= //

    // ======================================= Physical Device Begin ======================================= //
    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        std::multimap<int, VkPhysicalDevice> candidates;

        for (const auto& device : devices)
        {
            int score = rateDeviceSuitablility(device);
            candidates.insert(std::make_pair(score, device));

            // if (isDeviceSuitable(device))
            // {
            //     physicalDevice = device;
            //     break;
            // }
        }
        if (candidates.rbegin()->first > 0)
        {
            if (isDeviceSuitable(candidates.rbegin()->second))
            {
                physicalDevice = candidates.rbegin()->second;
            }
        }
        else
        {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = findeQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);
        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }

    int rateDeviceSuitablility(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        int score = 0;
        // 优先使用独显
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        score += (int)deviceProperties.limits.maxImageDimension2D;

        if (!deviceFeatures.geometryShader) return 0;
        std::cout << "GPU: " << deviceProperties.deviceName << ", Score: " << score << std::endl;
        return score;
    }

    QueueFamilyIndices findeQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                VkBool32 presentSurpport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSurpport);
                if (presentSurpport) indices.presentFamily = i;

                indices.graphicsFamily = i;
            }
            if (indices.isComplete()) break;
            i++;
        }

        return indices;
    }
    // ======================================= Physical Device End ======================================= //

    // ======================================= Logical Device Begin ======================================= //
    void createLogicalDevice()
    {
        QueueFamilyIndices indices = findeQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniquedQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniquedQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
    }
    // ======================================= Logical Device End ======================================= //

    // ======================================= Window surface creation Begin ======================================= //
    void createSurface()
    {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface!");
        }
    }
    // ======================================= Window surface creation End ======================================= //

    // ======================================= Swap Chain Begin ======================================= //
    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(logicalDevice);

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createFrameBuffers();
    }

    void cleanupSwapChain()
    {
        for (size_t i = 0; i < swapChainFrameBuffers.size(); i++)
        {
            vkDestroyFramebuffer(logicalDevice, swapChainFrameBuffers[i], nullptr);
        }
        for (size_t i = 0; i < swapChainImageViews.size(); i++)
        {
            vkDestroyImageView(logicalDevice, swapChainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
    }

    void createSwapChain()
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findeQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requireExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            requireExtensions.erase(extension.extensionName);
        }
        return requireExtensions.empty();
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != UINT_MAX)
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }
    // ======================================= Swap Chain End ======================================= //

    // ======================================= Image View Begin ======================================= //
    void createImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
        }
    }
    // ======================================= Image View End ======================================= //

    // ======================================= Shader Modules Begin ======================================= //
    VkShaderModule createShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module!");
        }
        return shaderModule;
    }

    static std::vector<char> readFiles(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file!" + filename);
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), (long long)fileSize);
        file.close();

        return buffer;
    }
    // ======================================= Shader Modules End ======================================= //

    // ======================================= Graphics Pipeline Begin ======================================= //
    void createGraphicsPipeline()
    {
        auto vsCode = readFiles("../Assets/Shaders/SPIRV/TextureMapping.vert.spv");
        auto psCode = readFiles("../Assets/Shaders/SPIRV/TextureMapping.frag.spv");

        VkShaderModule vsShaderModule = createShaderModule(vsCode);
        VkShaderModule psShaderModule = createShaderModule(psCode);

        VkPipelineShaderStageCreateInfo vsShaderStageInfo{};
        vsShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vsShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vsShaderStageInfo.module = vsShaderModule;
        vsShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo psShaderStageInfo{};
        psShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        psShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        psShaderStageInfo.module = psShaderModule;
        psShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStageInfo[] = { vsShaderStageInfo, psShaderStageInfo };

        // Vertex Input
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescription = Vertex::getAttributeDescription();
        VkPipelineVertexInputStateCreateInfo vsInputInfo{};
        vsInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vsInputInfo.vertexBindingDescriptionCount = 1;
        vsInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vsInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
        vsInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
        inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

        // Viewports and scissors
        VkPipelineViewportStateCreateInfo viewportCreateInfo{};
        viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportCreateInfo.viewportCount = 1;
        viewportCreateInfo.scissorCount = 1;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
        rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizerCreateInfo.depthClampEnable = VK_FALSE;
        rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizerCreateInfo.lineWidth = 1.0f;
        rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
        rasterizerCreateInfo.depthBiasConstantFactor = 0.0f;    // Optional
        rasterizerCreateInfo.depthBiasClamp = 0.0f;             // Optional
        rasterizerCreateInfo.depthBiasSlopeFactor = 0.0f;       // Optional

        // MultiSampling
        VkPipelineMultisampleStateCreateInfo multisampleCreateInfo{};
        multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
        multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampleCreateInfo.minSampleShading = 1.0f;          // Optional
        multisampleCreateInfo.pSampleMask = nullptr;            // Optional
        multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampleCreateInfo.alphaToOneEnable = VK_FALSE;      // Optional

        // Color Blending
        VkPipelineColorBlendAttachmentState colorBlendAttachments{};
        colorBlendAttachments.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachments.blendEnable = VK_FALSE;

        // 注释测试
        VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo{};
        colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendingCreateInfo.logicOpEnable = VK_FALSE;
        colorBlendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;   // Optional
        colorBlendingCreateInfo.attachmentCount = 1;
        colorBlendingCreateInfo.pAttachments = &colorBlendAttachments;
        colorBlendingCreateInfo.blendConstants[0] = 0.0f;     // Optional
        colorBlendingCreateInfo.blendConstants[1] = 0.0f;     // Optional
        colorBlendingCreateInfo.blendConstants[2] = 0.0f;     // Optional
        colorBlendingCreateInfo.blendConstants[3] = 0.0f;     // Optional

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = 2;
        pipelineCreateInfo.pStages = shaderStageInfo;
        pipelineCreateInfo.pVertexInputState = &vsInputInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
        pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
        pipelineCreateInfo.pDepthStencilState = nullptr;
        pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.layout = pipelineLayout;
        pipelineCreateInfo.renderPass = renderPass;
        pipelineCreateInfo.subpass = 0;
        pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineCreateInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(logicalDevice, psShaderModule, nullptr);
        vkDestroyShaderModule(logicalDevice, vsShaderModule, nullptr);
    }

    void createRenderPass()
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency subpassDependency{};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.srcAccessMask = 0;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 1;
        renderPassCreateInfo.pAttachments = &colorAttachment;
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpass;
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &subpassDependency;

        if (vkCreateRenderPass(logicalDevice, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass!");
        }
    }
    // ======================================= Graphics Pipeline End ======================================= //

    // ======================================= FrameBuffer Begin ======================================= //
    void createFrameBuffers()
    {
        swapChainFrameBuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); i++)
        {
            VkImageView attachments[] = { swapChainImageViews[i] };
            VkFramebufferCreateInfo frameBufferCreateInfo{};
            frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frameBufferCreateInfo.renderPass = renderPass;
            frameBufferCreateInfo.attachmentCount = 1;
            frameBufferCreateInfo.pAttachments = attachments;
            frameBufferCreateInfo.width = swapChainExtent.width;
            frameBufferCreateInfo.height = swapChainExtent.height;
            frameBufferCreateInfo.layers = 1;

            if (vkCreateFramebuffer(logicalDevice, &frameBufferCreateInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }
    // ======================================= FrameBuffer End ======================================= //

    // ======================================= CommmandBuffer Begin ======================================= //
    void createCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = findeQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo cmdPoolCreateInfo{};
        cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(logicalDevice, &cmdPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool!");
        }
    }

    void createCommandBuffer()
    {
        commandBuffers.resize(MAX_FRAME_IN_FLIGHT);

        VkCommandBufferAllocateInfo cmdBufferAllocInfo{};
        cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAllocInfo.commandPool = commandPool;
        cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(logicalDevice, &cmdBufferAllocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo cmdBufferBeginInfo{};
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufferBeginInfo.flags = 0;
        cmdBufferBeginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = swapChainFrameBuffers[imageIndex];
        renderPassBeginInfo.renderArea.offset = { 0, 0 };
        renderPassBeginInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = { {{0.1f, 0.1f, 0.1f, 1.0f}} };
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearColor;

        // Begin Render Pass
        {
            vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkBuffer vertexBuffers[] = { vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)swapChainExtent.width;
            viewport.height = (float)swapChainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = swapChainExtent;
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
            
            vkCmdBindDescriptorSets(cmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                0, 1, &descriptorSets[currentFrame],
                0, nullptr);
            
            // vkCmdDraw(cmdBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
            vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

            vkCmdEndRenderPass(cmdBuffer);
        }
        if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }
    // ======================================= CommmandBuffer End ======================================= //

    // ======================================= Rendering Begin ======================================= //
    void drawFrame()
    {
        vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }
        
        updateUniformBuffer(currentFrame);
        
        // 只有成功提交了才重置Fence
        vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frameBufferResized)
        {
            frameBufferResized = false;
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAME_IN_FLIGHT;
    }

    void createSyncObjects()
    {
        imageAvailableSemaphores.resize(MAX_FRAME_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAME_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAME_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create synchronization objects for a frame!");
            }
        }

    }
    // ======================================= Rendering End ======================================= //

    // ======================================= Vertex Buffers Creation Begin ======================================= //
    void createVertexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), bufferSize);
        vkUnmapMemory(logicalDevice, stagingBufferMemory);

        createBuffer(bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }
    // ======================================= Vertex Buffers Creation End ======================================= //

    // ======================================= Staging Buffer Begin ======================================= //
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags propertiesFlag, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memRequirements.size;
        allocateInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, propertiesFlag);

        if (vkAllocateMemory(logicalDevice, &allocateInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        
        endSingleTimeCommands(commandBuffer);
    }
    // ======================================= Staging Buffer End ======================================= //

    // ======================================= Index Buffer Begin ======================================= //
    void createIndexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), bufferSize);
        vkUnmapMemory(logicalDevice, stagingBufferMemory);

        createBuffer(bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
    }
    // ======================================= Index Buffer End ======================================= //

    // ======================================= Uniform Buffer Begin ======================================= //
    void createUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(MAX_FRAME_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAME_IN_FLIGHT);
        uniformBufferMapped.resize(MAX_FRAME_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
        {
            createBuffer(bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniformBuffers[i], uniformBuffersMemory[i]);

            vkMapMemory(logicalDevice, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBufferMapped[i]);
        }
    }
    
    void createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void createDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 2>  poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAME_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAME_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolCreateInfo.pPoolSizes = poolSizes.data();
        poolCreateInfo.maxSets = static_cast<uint32_t>(MAX_FRAME_IN_FLIGHT);

        if (vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void createDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layout(MAX_FRAME_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = descriptorPool;
        allocateInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAME_IN_FLIGHT);
        allocateInfo.pSetLayouts = layout.data();

        descriptorSets.resize(MAX_FRAME_IN_FLIGHT);
        if (vkAllocateDescriptorSets(logicalDevice, &allocateInfo, descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureImageView;
            imageInfo.sampler = textureSampler;
            
            std::array<VkWriteDescriptorSet, 2>  descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(uniformBufferMapped[currentImage], &ubo, sizeof(ubo));
    }
    // ======================================= Uniform Buffer End ======================================= //

    // ======================================= Texture Mapping Begin ======================================= //
    void createTextureImage()
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("../Assets/Textures/TextureMapping.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels)
        {
            throw std::runtime_error("Failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingBufferMemory);

        void * data;
        vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(logicalDevice, stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(texWidth, texHeight,
            VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

        // 先将图像转换成VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL，作为传输目标
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        // 传输完成后，转换为可以被Shader采样的图像布局
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags propertiesFlags, VkImage& image, VkDeviceMemory& imageMemory)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;    // Optional

        if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memRequirements.size;
        allocateInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, propertiesFlags);

        if (vkAllocateMemory(logicalDevice, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(logicalDevice, image, imageMemory, 0);
    }

    VkCommandBuffer beginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandPool = commandPool;
        allocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(logicalDevice, &allocateInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer cmdBuffer)
    {
        vkEndCommandBuffer(cmdBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
    
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
    
        vkFreeCommandBuffers(logicalDevice, commandPool, 1, &cmdBuffer);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage;
        VkPipelineStageFlags dstStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer, srcStage, dstStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        
        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        endSingleTimeCommands(commandBuffer);
    }
    // ======================================= Texture Mapping End ======================================= //

    // ======================================= Image View and Sampler Begin ======================================= //
    void createTextureImageView()
    {
        textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB);
    }

    VkImageView createImageView(VkImage image, VkFormat format)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &imageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image views!");
        }

        return imageView;
    }

    void createTextureSampler()
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        
        VkSamplerCreateInfo samplerCreaetInfo{};
        samplerCreaetInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreaetInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreaetInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreaetInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreaetInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreaetInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreaetInfo.anisotropyEnable = VK_TRUE;
        samplerCreaetInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerCreaetInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerCreaetInfo.unnormalizedCoordinates = VK_FALSE;
        samplerCreaetInfo.compareEnable = VK_FALSE;
        samplerCreaetInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerCreaetInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreaetInfo.mipLodBias = 0.0f;
        samplerCreaetInfo.minLod = 0.0f;
        samplerCreaetInfo.maxLod = 0.0f;

        if (vkCreateSampler(logicalDevice, &samplerCreaetInfo, nullptr, &textureSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create texture sampler!");
        }
        
    }
    // ======================================= Image View and Sampler End ======================================= //

private:
    // Setup
    GLFWwindow* window{};
    VkInstance instance{};
    VkDebugUtilsMessengerEXT debugMessenger{};
    VkPhysicalDevice physicalDevice{};
    VkDevice logicalDevice{};
    VkQueue graphicsQueue{};

    // Presentation
    VkSurfaceKHR surface{};
    VkQueue presentQueue{};
    VkSwapchainKHR swapChain{};
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapChainExtent = { 0, 0 };
    std::vector<VkImageView> swapChainImageViews;

    // Graphics Pipeline
    VkPipelineLayout pipelineLayout{};
    VkRenderPass renderPass{};
    VkPipeline graphicsPipeline{};

    // Drawing
    std::vector<VkFramebuffer> swapChainFrameBuffers;
    VkCommandPool commandPool{};

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    bool frameBufferResized = false;

    // Vertex Buffer
    VkBuffer vertexBuffer{};
    VkDeviceMemory vertexBufferMemory{};
    VkBuffer indexBuffer{};
    VkDeviceMemory indexBufferMemory{};

    // Uniform Buffer
    VkDescriptorSetLayout descriptorSetLayout{};
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBufferMapped;
    VkDescriptorPool descriptorPool{};
    std::vector<VkDescriptorSet> descriptorSets;

    // Texture Mapping
    VkImage textureImage{};
    VkDeviceMemory textureImageMemory{};
    VkImageView textureImageView{};
    VkSampler textureSampler{};
};

int main(int argc, char* argv[])
{
    HelloTriangleApplication app;

    try
    {
        app.Run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
