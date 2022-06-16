#include <SDL.h>

#include <helloworld/helloworld.hpp>
#include <sandbox/sandbox_app.hpp>

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    oblo::vk::sandbox_app<oblo::vk::helloworld> app;

    if (!app.init())
    {
        return false;
    }

    app.run();
    app.shutdown();

    SDL_Quit();

    return 0;
}