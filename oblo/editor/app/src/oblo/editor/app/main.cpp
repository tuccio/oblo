#include <SDL.h>

#include <oblo/core/platform/core.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/sandbox/sandbox_app.hpp>

#include "app.hpp"

namespace oblo
{
    // Occasionally useful when debugging the app launched by RenderDoc/Nsight
    [[maybe_unused]] static void wait_for_debugger()
    {
        while (!oblo::platform::is_debugger_attached())
        {
        }
    }
}

int SDL_main(int argc, char* argv[])
{
    oblo::module_manager moduleManager;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    oblo::vk::sandbox_app<oblo::editor::app> app;

    app.set_config({
        .imguiIniFile = "oblo.imgui.ini",
        .uiWindowMaximized = true,
        .uiUseDocking = true,
        .uiUseMultiViewport = true,
        .vkUseValidationLayers = false,
    });

    if (!app.init(argc, argv))
    {
        app.shutdown();
        return 1;
    }

    app.run();
    app.shutdown();

    SDL_Quit();

    return 0;
}