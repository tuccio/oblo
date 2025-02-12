#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo::log
{
    class log_sink;

    class log_module final : public module_interface
    {
    public:
        LOG_API bool startup(const module_initializer& initializer) override;
        LOG_API void shutdown() override;

        bool finalize() override
        {
            return true;
        }

        LOG_API void add_sink(unique_ptr<log_sink> sink);
    };
}