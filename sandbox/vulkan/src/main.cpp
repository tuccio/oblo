#include <SDL.h>

#include <sandbox/sandbox_app.hpp>

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    oblo::vk::sandbox_app app;

    if (!app.init())
    {
        return false;
    }

    app.run();
    app.wait_idle();

    SDL_Quit();

    return 0;
}