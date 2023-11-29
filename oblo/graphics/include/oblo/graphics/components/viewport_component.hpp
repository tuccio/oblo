#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/math/vec2.hpp>

namespace oblo
{
    using viewport_image_id = void*;

    struct picking_request
    {
        enum class state : u8
        {
            none,
            requested,
            awaiting,
            served,
            failed,
        };

        state state;

        union {
            // The screen-space coordinates for the picking, active when the state is requested.
            vec2 coordinates;

            // The picking result, active when the state is served.
            u32 result;
        };
    };

    struct viewport_component
    {
        u32 width;
        u32 height;
        viewport_image_id imageId;

        picking_request picking;
    };
}