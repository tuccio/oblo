#pragma once

#include <oblo/core/types.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo::vk
{
    struct surfel_metrics
    {
        u32 primaryRayCasts;
        u32 shadowRayCasts;
        u32 surfelsAlive;
        u32 surfelsSpawned;
        u32 surfelsKilled;
    } OBLO_REFLECT();
}