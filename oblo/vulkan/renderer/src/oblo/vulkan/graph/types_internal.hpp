#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    enum class pass_kind : u8
    {
        none,
        graphics,
        compute,
        raytracing,
        transfer,
    };
}