#pragma once

#include <oblo/modules/module_interface.hpp>

#include <memory>

namespace oblo::log
{
    class log_sink;

    class log_module final : public module_interface
    {
    public:
        LOG_API bool startup(const module_initializer& initializer) override;
        LOG_API void shutdown() override;

        LOG_API void add_sink(std::unique_ptr<log_sink> sink);
    };
}