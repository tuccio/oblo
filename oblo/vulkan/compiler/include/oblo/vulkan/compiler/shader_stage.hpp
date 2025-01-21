#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    enum class shader_stage : u8
    {
        mesh,
        task,
        compute,
        vertex,
        geometry,
        tessellation_control,
        tessellation_evaluation,
        fragment,
        raygen,
        intersection,
        closest_hit,
        any_hit,
        miss,
        callable,
    };
}