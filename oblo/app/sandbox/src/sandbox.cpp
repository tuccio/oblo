#include <oblo/app/graphics_engine.hpp>
#include <oblo/app/graphics_window.hpp>
#include <oblo/app/imgui_app.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/log/log_module.hpp>
#include <oblo/log/sinks/file_sink.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <imgui.h>

namespace oblo
{
    class app_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            initializer.services->add<resource_registry>().unique();
            module_manager::get().load<log::log_module>()->add_sink(allocate_unique<log::file_sink>(stdout));
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
    mm.load<app_module>();

    mm.finalize();

    graphics_window mainWindow;

    if (!mainWindow.create({.title = "Sandbox"}) || !mainWindow.initialize_graphics())
    {
        return 1;
    }

    auto* const gfxEngine = mm.find_unique_service<graphics_engine>();

    if (!gfxEngine)
    {
        return 2;
    }

    imgui_app app;

    if (!app.init(mainWindow))
    {
        return 3;
    }

    window_event_processor eventProcessor{imgui_app::get_event_dispatcher()};

    while (mainWindow.is_open() && eventProcessor.process_events())
    {
        if (!gfxEngine->acquire_images())
        {
            // Can decide what to do, e.g. run the update
            continue;
        }

        // TODO: Render
        app.begin_frame();

        ImGui::ShowDemoWindow();
        app.render();

        gfxEngine->present();

        app.end_frame();
    }

    return 0;
}