#include <oblo/window/window_module.hpp>

#include <oblo/modules/module_manager.hpp>

namespace oblo
{
    bool window_module::startup(const module_initializer&)
    {
        return true;
    }

    void window_module::shutdown() {}

    void window_module::finalize() {}

    window_event_processor window_module::create_event_processor()
    {
        window_event_dispatcher eventDispatcher{};

        auto* const dispatcher = module_manager::get().find_unique_service<window_event_dispatcher>();

        if (dispatcher)
        {
            eventDispatcher = *dispatcher;
        }

        return window_event_processor{eventDispatcher};
    }
}