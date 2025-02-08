#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    class graphics_window;
    class graphics_window_context;
    using native_window_handle = void*;

    class graphics_engine
    {
    public:
        virtual ~graphics_engine() = default;

        virtual graphics_window_context* create_context(native_window_handle wh, u32 width, u32 height) = 0;

        virtual bool acquire_images() = 0;
        virtual void present() = 0;
    };
}