#pragma once

#include <oblo/editor/asset_browser.hpp>
#include <oblo/editor/window.hpp>

namespace oblo::editor
{
    class main_window final : public window
    {
    public:
        bool update() override;
    };
}