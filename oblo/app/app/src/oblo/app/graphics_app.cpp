#include <oblo/app/graphics_app.hpp>

#include <oblo/app/graphics_engine.hpp>
#include <oblo/modules/module_manager.hpp>

namespace oblo
{
    graphics_app::graphics_app() = default;

    graphics_app::~graphics_app()
    {
        shutdown();
    }

    expected<> graphics_app::init(const graphics_window_initializer& initializer)
    {
        m_graphicsEngine = module_manager::get().find_unique_service<graphics_engine>();

        if (!m_graphicsEngine)
        {
            return unspecified_error;
        }

        if (!m_mainWindow.create(initializer) || !m_mainWindow.initialize_graphics())
        {
            return unspecified_error;
        }

        return no_error;
    }

    void graphics_app::shutdown()
    {
        m_mainWindow.destroy();
    }

    bool graphics_app::process_events()
    {
        return m_eventProcessor.process_events() && m_mainWindow.is_open();
    }

    bool graphics_app::acquire_images()
    {
        return m_graphicsEngine->acquire_images();
    }

    void graphics_app::present()
    {
        m_graphicsEngine->present();
    }

    graphics_window& graphics_app::get_main_window()
    {
        return m_mainWindow;
    }

    void graphics_app::set_input_queue(input_queue* inputQueue)
    {
        m_eventProcessor.set_input_queue(inputQueue);
    }
}