#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo::sandbox
{
    OBLO_FUNCTION(ScriptAPI)
    OBLO_SCRIPTING_SANDBOX_API void log_from_native();

    OBLO_FUNCTION(ScriptAPI)
    OBLO_SCRIPTING_SANDBOX_API f32 compute_scaled(i32 count, f32 scale, vec3 offset, bool enabled);
}