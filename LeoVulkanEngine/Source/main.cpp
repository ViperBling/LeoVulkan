#include <Engine/VkEngine.h>

int main(int argc, char* argv[])
{
    LeoVKEngine Engine;

    Engine.Init();
    Engine.Run();
    Engine.CleanUp();

    return 0;
}