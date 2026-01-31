#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo::importers
{
    class importers_module final : public module_interface
    {
    public:
        OBLO_IMPORTERS_API bool startup(const module_initializer& initializer) override;
        OBLO_IMPORTERS_API void shutdown() override;
        bool finalize() override { return true; }
    };
}