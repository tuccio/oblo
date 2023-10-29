#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo::importers
{
    class IMPORTERS_API importers_module final : public module_interface
    {
    public:
        bool startup() override;
        void shutdown() override;
    };
}