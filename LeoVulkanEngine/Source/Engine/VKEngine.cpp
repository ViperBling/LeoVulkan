#include "VkEngine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <VKTypes.h>
#include <VKInitializers.h>

void LeoVKEngine::Init()
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags wndFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    window = SDL_CreateWindow(
        "LeoVKEngine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        (int)windowExtent.width,
        (int)windowExtent.height,
        wndFlags
        );
    bIsInitialized = true;
}

void LeoVKEngine::CleanUp()
{
    if (bIsInitialized)
    {
        SDL_DestroyWindow(window);
    }
}

void LeoVKEngine::Draw()
{

}

void LeoVKEngine::Run()
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
