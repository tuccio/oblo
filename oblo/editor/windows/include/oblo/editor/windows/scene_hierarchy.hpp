#pragma once

#include <oblo/core/types.hpp>

namespace oblo::editor
{
    class editor_world;
    struct window_update_context;

    class scene_hierarchy final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

    private:
        editor_world* m_editorWorld;
        u32 m_lastRefreshEvent{};
    };
}