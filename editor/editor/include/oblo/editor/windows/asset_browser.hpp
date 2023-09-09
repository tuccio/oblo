#pragma once

#include <oblo/editor/window.hpp>

namespace oblo::editor
{
    class asset_browser final : public window
    {
    public:
        bool update() override;
    };
}