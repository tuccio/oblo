#include <SDL.h>

#include <oblo/sandbox/sandbox_app.hpp>

#include <vertex_pull/vertex_pull.hpp>

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    oblo::vk::sandbox_app<oblo::vk::vertex_pull> app;

    if (!app.init())
    {
        app.shutdown();
        return 1;
    }

    app.run();
    app.shutdown();

    SDL_Quit();

    return 0;
}