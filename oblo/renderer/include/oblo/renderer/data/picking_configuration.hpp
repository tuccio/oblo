#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/vec2.hpp>

namespace oblo
{
    // The size of the square region around the cursor that gets rendered for picking, in pixels.
    // This is a compile-time knob that can be bumped up for debugging purposes.
    constexpr u32 PickingRegionSize = 1;

    // Maximum number of entities that can be excluded from picking in a single request.
    constexpr u32 MaxPickingExcludedEntities = 16;

    struct picking_configuration
    {
        // The viewport-local coordinates of the pixel to pick.
        vec2 coordinates;

        // Entities to skip while rendering the picking pass, their ids match the ones stored in i_EntityIdBuffer.
        u32 excludedEntityCount{};
        u32 excludedEntityIds[MaxPickingExcludedEntities]{};
    };
}
