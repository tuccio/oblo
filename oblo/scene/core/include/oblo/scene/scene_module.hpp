#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class scene_module final : public module_interface
    {
    public:
        SCENE_API bool startup(const module_initializer& initializer) override;
        SCENE_API void shutdown() override;
        bool finalize() override { return true; }
    };
}