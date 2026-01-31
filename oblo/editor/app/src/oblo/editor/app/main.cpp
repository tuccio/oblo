#include <oblo/core/platform/core.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/editor/app/app.hpp>
#include <oblo/editor/app/launcher.hpp>

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

int main(int argc, char* argv[])
{
    struct error_code
    {
        enum values
        {
            ok,
            platform_init_failed,
            launch_cancelled,
            editor_startup_failed,
        };
    };

    if (!oblo::platform::init())
    {
        return error_code::platform_init_failed;
    }

    const auto platformShutdown = oblo::finally([] { oblo::platform::shutdown(); });

    oblo::editor::run_config runConfig;

    {
        oblo::editor::launcher launcher;

        if (!launcher.run(argc, argv, runConfig))
        {
            return error_code::launch_cancelled;
        }
    }

    {
        oblo::editor::app editorApp;

        if (!editorApp.init(runConfig))
        {
            return error_code::editor_startup_failed;
        }

        editorApp.run();
    }

    return error_code::ok;
}