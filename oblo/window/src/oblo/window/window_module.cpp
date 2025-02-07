#include <oblo/window/window_module.hpp>

namespace oblo
{
    bool window_module::startup(const module_initializer&)
    {
        m_mainWindow.create({
            .title = "oblo",
            .isHidden = true,
        });

        return true;
    }

    void window_module::shutdown() {}

    void window_module::finalize() {}

    graphics_window& window_module::get_main_window()
    {
        return m_mainWindow;
    }
}