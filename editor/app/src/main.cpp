#include <SDL.h>

#include <oblo/sandbox/sandbox_app.hpp>

#include "app.hpp"

int SDL_main(int, char*[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    oblo::vk::sandbox_app<oblo::editor::app> app;

    app.set_config({
        .uiUseDocking = true,
        .uiUseMultiViewport = true,
        .vkUseValidationLayers = false,
    });

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