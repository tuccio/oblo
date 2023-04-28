#include <SDL.h>

#include <sandbox/sandbox_app.hpp>

#include <hello_world/hello_world.hpp>
#include <renderer/renderer.hpp>
#include <vertex_pull/vertex_pull.hpp>

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    oblo::vk::sandbox_app<oblo::vk::renderer> app;

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