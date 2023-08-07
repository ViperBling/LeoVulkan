#include <SDL.h>
#include <SDL_vulkan.h>

#include "VKEngine.hpp"
#include "VKTypes.hpp"
#include "VKInitializers.hpp"

void VulkanEngine::Init()
{
    SDL_Init(SDL_INIT_VIDEO);

    auto wndFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    mWnd = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        mWndExtent.width,
        mWndExtent.height,
        wndFlags
        );
    mb_Initialized = true;
}

void VulkanEngine::CleanUp()
{
    if (mb_Initialized) SDL_DestroyWindow(mWnd);
}

void VulkanEngine::Draw()
{

}

void VulkanEngine::Run()
{
    SDL_Event event;
    bool bQuit = false;

    while (!bQuit)
    {
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT) bQuit = true;
        }
        Draw();
    }
}
