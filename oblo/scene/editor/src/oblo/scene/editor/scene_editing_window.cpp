#include <oblo/scene/editor/scene_editing_window.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/runtime_manager.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/editor/windows/command_palette_window.hpp>
#include <oblo/editor/windows/inspector.hpp>
#include <oblo/editor/windows/scene_hierarchy.hpp>
#include <oblo/editor/windows/viewport.hpp>
#include <oblo/runtime/runtime.hpp>

#include <imgui.h>

namespace oblo::editor
{
    bool scene_editing_window::init(const window_update_context& ctx)
    {
        m_runtimeManager = ctx.services.find<runtime_manager>();
        OBLO_ASSERT(m_runtimeManager);

        m_runtime = m_runtimeManager->create();

        if (!m_runtime)
        {
            return false;
        }

        auto* const registry = ctx.services.get_local_registry();
        OBLO_ASSERT(registry);

        registry->add<selected_entities>().externally_owned(&m_selection);

        registry->add<ecs::entity_registry>().externally_owned(&m_runtime->get_entity_registry());
        registry->add<scene_renderer>().externally_owned(m_runtime->get_service_registry().find<scene_renderer>());

        ctx.windowManager.create_child_window<inspector>(ctx.windowHandle);
        ctx.windowManager.create_child_window<scene_hierarchy>(ctx.windowHandle);
        ctx.windowManager.create_child_window<viewport>(ctx.windowHandle);

        return true;
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

    void scene_editing_window::on_close()
    {
        if (m_runtime)
        {
            m_runtimeManager->destroy(m_runtime);
            m_runtime = {};
        }
    }
}