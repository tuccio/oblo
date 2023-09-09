#pragma once

#include <oblo/editor/window.hpp>

namespace oblo::editor
{
    class dock_space final : public window
    {
    public:
        bool update() override;
    };
}