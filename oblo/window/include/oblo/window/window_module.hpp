#pragma once

#include <oblo/modules/module_interface.hpp>
#include <oblo/window/graphics_window.hpp>

namespace oblo
{
    class window_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        void finalize() override;

        graphics_window& get_main_window();

    private:
        graphics_window m_mainWindow;
    };
}