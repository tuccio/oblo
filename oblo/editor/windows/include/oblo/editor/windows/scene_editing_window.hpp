#pragma once

#include <oblo/editor/services/selected_entities.hpp>

namespace oblo::editor
{
    struct window_update_context;

    class scene_editing_window final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        selected_entities m_selection;
    };
}