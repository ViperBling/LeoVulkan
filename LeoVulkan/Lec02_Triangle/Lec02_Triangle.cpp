#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <glm/ext/scalar_uint_sized.hpp>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

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
    void initWindow()
    {
        glfwInit();

        // 设置窗口属性
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        // 创建窗口
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }
    
    void initVulkan()
    {
        createInstance();
    }
    
    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }
    
    void cleanup()
    {
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance()
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engines";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;
        
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        std::vector<VkExtensionProperties> extensions(glfwExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &glfwExtensionCount, extensions.data());
        std::cout << "Avaliable extensions: \n";
        for (const auto & extension : extensions)
        {
            std::cout << '\t' << extension.extensionName << '\n';
        }
        
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0;

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create instance!");
        } 
        
    }

private:
    GLFWwindow* window = nullptr;
    VkInstance instance = NULL;
};

int main(int argc, char* argv[])
{
    HelloTriangleApplication app;

    try
    {
        app.Run();
    } catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
