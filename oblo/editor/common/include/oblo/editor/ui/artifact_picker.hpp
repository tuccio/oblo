#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <imgui.h>

namespace oblo
{
    class resource_registry;
    class asset_registry;

    namespace editor
    {
        class asset_editor_manager;
        class window_manager;
    }
}

namespace oblo::editor::ui
{
    class artifact_picker
    {
    public:
        explicit artifact_picker(asset_registry& registry, asset_editor_manager* assetEditors);
        explicit artifact_picker(const resource_registry& registry, asset_editor_manager* assetEditors);

        bool draw(int uiId, const uuid& type, const uuid& ref);

        uuid get_current_ref() const;

        void set_window_manager(window_manager* wm);

    private:
        asset_registry* m_assetRegistry{};
        const resource_registry* m_resourceRegistry{};
        window_manager* m_windowManager{};
        asset_editor_manager* m_assetEditors{};
        uuid m_currentRef{};
        ImGuiTextFilter m_textFilter;
    };
}