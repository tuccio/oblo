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
#include <oblo/resource/providers/resource_types_provider.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/resource/utility/registration.hpp>
#include <oblo/scene/scene_module.hpp>
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
        bool finalize() override { return true; }
    };
}

int main(int, char**)
{
    using namespace oblo;

    module_manager mm;

    mm.load<vk::vulkan_engine_module>();
    mm.load<app_module>();
    mm.load<scene_module>();

    mm.finalize();

    auto* resourceRegistry = mm.find_unique_service<resource_registry>();
    register_resource_types(*resourceRegistry, mm.find_services<resource_types_provider>());

    imgui_app app;

    if (!app.init({.title = "Sandbox"}) || !app.init_font_atlas(*resourceRegistry))
    {
        return 3;
    }

    while (app.process_events())
    {
        if (!app.acquire_images())
        {
            // Can decide what to do, e.g. run the update
            continue;
        }

        app.begin_ui();
        ImGui::ShowDemoWindow();
        app.end_ui();

        app.present();
    }

    return 0;
}