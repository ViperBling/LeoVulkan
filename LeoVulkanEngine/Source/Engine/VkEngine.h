#pragma once

#include <VKTypes.h>

class LeoVKEngine
{
public:

    void Init();
    void CleanUp();
    void Draw();
    void Run();

public:
    bool bIsInitialized {false};
    int frameNumber {0};

    VkExtent2D windowExtent {1280, 720};

    struct SDL_Window* window {nullptr};
};