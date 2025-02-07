#include <oblo/modules/module_manager.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>
#include <oblo/window/graphics_engine.hpp>
#include <oblo/window/graphics_window.hpp>
#include <oblo/window/window_module.hpp>

int main(int, char**)
{
    using namespace oblo;

    module_manager mm;

    mm.load<vk::vulkan_engine_module>();
    auto* wm = mm.load<window_module>();

    mm.finalize();

    graphics_window& mainWindow = wm->get_main_window();

    if (!mainWindow.initialize_graphics())
    {
        return 1;
    }

    auto* const gfxEngine = mm.find_unique_service<graphics_engine>();

    if (!gfxEngine)
    {
        return 2;
    }

    while (mainWindow.is_open())
    {
        mainWindow.update();

        if (!gfxEngine->acquire_images())
        {
            // Can decide what to do, e.g. run the update
            continue;
        }

        // TODO: Render

        gfxEngine->present();
    }

    return 0;
}