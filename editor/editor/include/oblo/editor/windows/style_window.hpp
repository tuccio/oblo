#pragma once

#include <oblo/editor/window.hpp>

namespace oblo::editor
{
    class style_window final : public window
    {
        bool update() override;
    };
}