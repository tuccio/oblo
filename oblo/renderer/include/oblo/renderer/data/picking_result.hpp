#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    // Matches the std430 layout of the picking_result struct in entity_picking.comp
    struct picking_result
    {
        vec3 position;
        u32 entityId;
        vec3 normal;
        f32 _padding;
    };
}
