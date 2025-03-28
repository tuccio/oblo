#include <oblo/editor/windows/scene_editing_window.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/editor/windows/command_palette_window.hpp>
#include <oblo/editor/windows/demo_window.hpp>
#include <oblo/editor/windows/frame_graph_window.hpp>
#include <oblo/editor/windows/options_editor.hpp>
#include <oblo/editor/windows/style_window.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/vulkan/events/gi_reset_event.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/renderer.hpp>

#include <imgui.h>

namespace oblo::editor
{
    void scene_editing_window::init(const window_update_context& ctx)
    {
        auto* const registry = ctx.services.get_local_registry();
        OBLO_ASSERT(registry);

        registry->add<selected_entities>().externally_owned(&m_selection);
    }

    bool scene_editing_window::update(const window_update_context& ctx)
    {
        if (auto& io = ImGui::GetIO(); !io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P))
        {
            ctx.windowManager.create_child_window<command_palette_window>(ctx.windowHandle,
                window_flags::unique_sibling,
                {});
        }

        return true;
    }
}