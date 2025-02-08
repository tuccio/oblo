#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/window/graphics_window.hpp>
#include <oblo/window/window_event_processor.hpp>

namespace oblo
{
    class window_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        void finalize() override;

        window_event_processor create_event_processor();
    };
}