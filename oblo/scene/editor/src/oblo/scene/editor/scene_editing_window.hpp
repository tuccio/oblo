#pragma once

#include <oblo/editor/services/selected_entities.hpp>

namespace oblo
{
    class runtime_manager;
    class runtime;
}

namespace oblo::editor
{
    struct window_update_context;

    class scene_editing_window final
    {
    public:
        ~scene_editing_window();

        bool init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);
        void on_close();

    private:
        selected_entities m_selection;
        runtime_manager* m_runtimeManager{};
        runtime* m_runtime{};
    };
}