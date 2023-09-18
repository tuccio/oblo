#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo::scene
{
    class SCENE_API scene_module final : public module_interface
    {
    public:
        bool startup() override;
        void shutdown() override;
    };
}