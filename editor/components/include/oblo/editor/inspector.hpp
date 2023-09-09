#pragma once

#include <oblo/editor/window.hpp>

namespace oblo::editor
{
    class inspector final : public window
    {
    public:
        bool update() override;
    };
}