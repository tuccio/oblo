#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class scene_module final : public module_interface
    {
    public:
        OBLO_SCENE_API bool startup(const module_initializer& initializer) override;
        OBLO_SCENE_API void shutdown() override;
        bool finalize() override { return true; }
    };
}