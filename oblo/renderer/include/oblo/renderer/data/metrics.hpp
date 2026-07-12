#pragma once

#include <oblo/core/types.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct surfel_metrics
    {
        u32 primaryRayCasts;
        u32 shadowRayCasts;
        u32 surfelsAlive;
        u32 surfelsSpawned;
        u32 surfelsKilled;
        u32 hashAcquireSuccess;
        u32 hashCollisions;
        u32 hashFailures;
    } OBLO_REFLECT();
}