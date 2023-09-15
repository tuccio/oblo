#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo::asset::importers
{
    class IMPORTERS_API module final : public module_interface
    {
    public:
        bool startup() override;
        void shutdown() override;
    };
}