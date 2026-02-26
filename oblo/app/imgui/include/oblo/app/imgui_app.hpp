#pragma once

#include <oblo/app/graphics_app.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/unique_ptr.hpp>

struct ImGuiContext;

namespace oblo
{
    class frame_graph;
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

    struct imgui_context
    {
        void* handle;
    };

    class imgui_app : graphics_app
    {
    public:
        imgui_app();
        imgui_app(const imgui_app&) = delete;
        imgui_app(imgui_app&&) noexcept = delete;
        ~imgui_app();

        imgui_app& operator=(const imgui_app&) = delete;
        imgui_app& operator=(imgui_app&&) noexcept = delete;

        expected<> init(
            const graphics_window_initializer& initializer, frame_graph& frameGraph, const imgui_app_config& cfg = {});

        void shutdown();

        expected<> init_font_atlas(const resource_registry& resourceRegistry);

        void begin_ui();
        void end_ui();

        using graphics_app::set_input_queue;

        using graphics_app::get_main_window;

        using graphics_app::process_events;

        using graphics_app::acquire_images;
        using graphics_app::present;

        ImGuiContext* get_imgui_context() const;

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };
}