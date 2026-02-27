#pragma once

#include <oblo/core/types.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct light_visibility_event
    {
        pin::texture resource;
        u32 lightIndex;
    };
}