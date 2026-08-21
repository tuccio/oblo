#pragma once

#include <oblo/core/types.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo::sandbox
{
    // Initializes the UI layout sandbox. Safe to call multiple times.
    // Called automatically by the ui_layout_sandbox_system, and callable from scripts.
    OBLO_FUNCTION(ScriptAPI)
    OBLO_UI_LAYOUT_SANDBOX_API void ui_layout_init();

    // Advances the sandbox layout by dt seconds. Requires ui_layout_init() to have been called.
    // Called automatically by the ui_layout_sandbox_system, and callable from scripts.
    OBLO_FUNCTION(ScriptAPI)
    OBLO_UI_LAYOUT_SANDBOX_API void ui_layout_update(f32 dt);
}