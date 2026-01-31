#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo::log
{
    class log_sink;

    class log_module final : public module_interface
    {
    public:
        OBLO_LOG_API bool startup(const module_initializer& initializer) override;
        OBLO_LOG_API void shutdown() override;

        bool finalize() override
        {
            return true;
        }

        OBLO_LOG_API void add_sink(unique_ptr<log_sink> sink);
    };
}