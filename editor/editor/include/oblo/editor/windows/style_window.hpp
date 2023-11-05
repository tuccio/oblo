#pragma once

namespace oblo::editor
{
    struct window_update_context;
    
    class style_window final
    {
    public:
        bool update(const window_update_context&);
    };
}