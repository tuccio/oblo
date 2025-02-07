#pragma once

namespace oblo
{
    class graphics_window;
    class graphics_window_context;

    class graphics_engine
    {
    public:
        virtual ~graphics_engine() = default;

        virtual graphics_window_context* create_context(const graphics_window& window) = 0;

        virtual bool acquire_images() = 0;
        virtual void present() = 0;
    };
}