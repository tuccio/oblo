#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class graphics_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        void finalize() override {}
    };
}