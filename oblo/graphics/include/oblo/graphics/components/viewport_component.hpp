#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/math/vec2.hpp>

namespace oblo
{
    namespace vk
    {
        struct frame_graph_subgraph;
    }

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
        gi_surfels_lighting,
        gi_surfels_raycount,
        gi_surfels_inconsistency,
    } OBLO_ENUM();

    struct viewport_component
    {
        u32 width;
        u32 height;

        viewport_mode mode;

        h32<vk::frame_graph_subgraph> graph;

        picking_request picking;
    } OBLO_COMPONENT();

    string_view get_viewport_mode_graph_output(viewport_mode mode);
}