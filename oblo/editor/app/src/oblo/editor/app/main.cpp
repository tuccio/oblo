#include <SDL.h>

#include <oblo/core/platform/core.hpp>
#include <oblo/modules/module_manager.hpp>

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
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    oblo::editor::app editorApp;

    if (!editorApp.init(argc, argv))
    {
        return 1;
    }

    editorApp.run();

    SDL_Quit();

    return 0;
}