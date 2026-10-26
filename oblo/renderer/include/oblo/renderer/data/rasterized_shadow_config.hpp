#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    enum class gpu_light_type : u32;

    struct rasterized_shadow_config
    {
        u32 lightIndex;
        gpu_light_type type;
    };
}