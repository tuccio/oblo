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

    enum class viewport_mode : u8
    {
        lit,
        albedo,
        normal_map,
        normals,
        tangents,
        bitangents,
        uv0,
        meshlet,
        metalness,
        roughness,
        emissive,
        motion_vectors,
        raytracing_debug,
        gi_surfels,
        gi_surfels_tile_coverage,
    };

    struct viewport_component
    {
        u32 width;
        u32 height;
        viewport_image_id imageId;

        picking_request picking;

        viewport_mode mode;
    };
}