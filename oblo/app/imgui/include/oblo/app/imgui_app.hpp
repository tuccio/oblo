#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo
{
    class graphics_window;
    class resource_registry;
    struct window_event_dispatcher;

    struct imgui_app_config
    {
        const char* configFile{"imgui.ini"};
        bool useMultiViewport{true};
        bool useDocking{true};
        bool useKeyboardNavigation{true};
    };

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

        expected<> init(const graphics_window& window, const imgui_app_config& cfg = {});
        void shutdown();

        expected<> init_font_atlas(const resource_registry& resourceRegistry);

        void begin_frame();
        void end_frame();

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };
}