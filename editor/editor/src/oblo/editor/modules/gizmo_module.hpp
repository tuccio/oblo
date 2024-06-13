#pragma once

#include <oblo/editor/window_module.hpp>

namespace oblo::editor
{
    class gizmo_module final : public window_module
    {
    public:
        void init();
        void update();
    };
}