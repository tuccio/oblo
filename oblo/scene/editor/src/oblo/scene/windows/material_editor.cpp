#include <oblo/scene/windows/material_editor.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <imgui.h>

namespace oblo::editor
{
    void material_editor::init(const window_update_context& ctx)
    {
        (void) ctx;
    }

    bool material_editor::update(const window_update_context& ctx)
    {
        (void) ctx;

        bool isOpen = true;
        ImGui::ShowDemoWindow(&isOpen);

        return isOpen;
    }
}