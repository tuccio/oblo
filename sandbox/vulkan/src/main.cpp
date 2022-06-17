#include <SDL.h>

#include <sandbox/sandbox_app.hpp>

#include <helloworld/helloworld.hpp>
#include <vertexpull/vertexpull.hpp>

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    oblo::vk::sandbox_app<oblo::vk::vertexpull> app;

    if (!app.init())
    {
        return false;
    }

    app.run();
    app.shutdown();

    SDL_Quit();

    return 0;
}