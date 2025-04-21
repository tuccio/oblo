#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/math/vec2u.hpp>

#include <span>

namespace oblo
{
    class graphics_app;
    class graphics_window_context;
    class window_event_processor;

    using native_window_handle = void*;

    enum class window_style : u8
    {
        game,
        app,
    };

    struct graphics_window_initializer
    {
        cstring_view title;
        u32 windowWidth{};
        u32 windowHeight{};
        window_style style{window_style::game};
        bool isHidden{};
        bool isMaximized{};
        bool isBorderless{};
    };

    enum class hit_test_result
    {
        normal,
        draggable,
        resize_top_left,
        resize_top,
        resize_top_right,
        resize_right,
        resize_bottom_right,
        resize_bottom,
        resize_bottom_left,
        resize_left
    };

    using hit_test_fn = function_ref<hit_test_result(const vec2u&)>;

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

        bool is_maximized() const;
        bool is_minimized() const;

        void maximize();
        void minimize();
        void restore();

        bool is_hidden() const;
        void set_hidden(bool hide);

        void set_custom_hit_test(const hit_test_fn* f);

        vec2u get_size() const;

        native_window_handle get_native_handle() const;

        void set_icon(u32 w, u32 h, std::span<const byte> data);

    private:
        friend class graphics_app;
        friend class window_event_processor;

    private:
        void* m_impl{};
        graphics_window_context* m_graphicsContext{};
        hit_test_fn m_hitTest{};
    };
}