#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <imgui.h>

namespace oblo
{
    class asset_registry;
}

namespace oblo::editor
{
    class artifact_picker
    {
    public:
        explicit artifact_picker(asset_registry& registry);

        bool draw(int uiId, const uuid& type, const uuid& ref);

        uuid get_current_ref() const;

    private:
        asset_registry& m_assetRegistry;
        uuid m_currentRef{};
        ImGuiTextFilter m_textFilter;
    };
}