#pragma once

#include "VKTypes.hpp"

class VulkanEngine
{
public:
    void Init();
    void CleanUp();
    void Draw();
    void Run();

public:
    bool mb_Initialized {false};
    int mFrameIndex {0};

    VkExtent2D mWndExtent {1280, 720};
    struct SDL_Window* mWnd {nullptr};
};