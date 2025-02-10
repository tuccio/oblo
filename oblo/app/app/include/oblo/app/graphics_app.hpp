#pragma once

#include <oblo/app/graphics_window.hpp>
#include <oblo/app/window_event_processor.hpp>
#include <oblo/core/expected.hpp>

namespace oblo
{
    class graphics_engine;
    struct graphics_window_initializer;

    class graphics_app
    {
    public:
        graphics_app();
        graphics_app(const graphics_app&) = delete;
        graphics_app(graphics_app&&) noexcept = delete;
        ~graphics_app();

        graphics_app& operator=(const graphics_app&) = delete;
        graphics_app& operator=(graphics_app&&) noexcept = delete;

        expected<> init(const graphics_window_initializer& initializer);
        void shutdown();

        [[nodiscard]] bool process_events();

        [[nodiscard]] bool acquire_images();
        void present();

        graphics_window& get_main_window();

        void set_input_queue(input_queue* inputQueue);

    protected:
        graphics_window m_mainWindow;
        window_event_processor m_eventProcessor;
        graphics_engine* m_graphicsEngine{};
    };
}