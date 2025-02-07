#pragma once

namespace oblo
{
    class graphics_window_context
    {
    public:
        virtual ~graphics_window_context() = default;

        virtual void on_resize(u32 width, u32 height) = 0;
        virtual void on_destroy() = 0;
    };
}