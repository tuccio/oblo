#include <oblo/core/service_registry.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>
#include <oblo/window/graphics_engine.hpp>
#include <oblo/window/graphics_window.hpp>
#include <oblo/window/window_module.hpp>

namespace oblo
{
    class app_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            initializer.services->add<resource_registry>().unique();
            return true;
        }

        void shutdown() override {}
        void finalize() override {}
    };
}

int main(int, char**)
{
    using namespace oblo;

    module_manager mm;

    mm.load<vk::vulkan_engine_module>();
    auto* wm = mm.load<window_module>();
    mm.load<app_module>();

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

    if (mainWindow.is_open())
    {
        mainWindow.set_hidden(false);
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