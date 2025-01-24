#pragma once

namespace oblo::editor
{
    struct window_update_context;

    class material_editor final
    {
    public:
        void init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);
    };
}