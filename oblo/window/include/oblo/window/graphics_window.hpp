#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/math/vec2u.hpp>

namespace oblo
{
    class graphics_window_context;

    using native_window_handle = void*;

    struct graphics_window_initializer
    {
        cstring_view title;
        u32 windowWidth;
        u32 windowHeight;
        bool isHidden;
        bool isMaximized;
    };

    class graphics_window
    {
    public:
        graphics_window();
        graphics_window(const graphics_window&) = delete;
        graphics_window(graphics_window&&) noexcept;

        ~graphics_window();

        graphics_window& operator=(const graphics_window&) = delete;
        graphics_window& operator=(graphics_window&&) noexcept;

        bool create(const graphics_window_initializer& initializer);
        void destroy();

        bool initialize_graphics();

        bool is_ready() const;
        bool is_open() const;

        bool is_hidden() const;
        void set_hidden(bool hide);

        vec2u get_size() const;

        void update();

        native_window_handle get_native_handle() const;

    private:
        void* m_impl{};
        graphics_window_context* m_graphicsContext{};
    };
}