#include <oblo/log/log_module.hpp>

#include <oblo/log/log_internal.hpp>

namespace oblo::log
{
    bool log_module::startup(const module_initializer&)
    {
        return true;
    }

    void log_module::shutdown()
    {
        g_logSinks.clear();
        g_logSinks.shrink_to_fit();
    }

    void log_module::add_sink(std::unique_ptr<log_sink> sink)
    {
        g_logSinks.push_back(std::move(sink));
    }
}