#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo
{
    class graphics_window;
    struct window_event_dispatcher;

    class imgui_app
    {
    public:
        static window_event_dispatcher get_event_dispatcher();

    public:
        imgui_app();
        imgui_app(const imgui_app&) = delete;
        imgui_app(imgui_app&&) noexcept = delete;
        ~imgui_app();

        imgui_app& operator=(const imgui_app&) = delete;
        imgui_app& operator=(imgui_app&&) noexcept = delete;

        expected<> init(const graphics_window& window);
        void shutdown();

        expected<> init_font_atlas();

        void begin_frame();
        void end_frame();

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };
}