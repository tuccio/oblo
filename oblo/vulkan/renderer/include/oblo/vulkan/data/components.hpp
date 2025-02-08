#pragma once

#include <oblo/vulkan/data/handles.hpp>

namespace oblo::vk
{
    struct draw_mesh;

    struct draw_instance_component
    {
        mesh_handle mesh;
    };

    struct draw_instance_id_component
    {
        // We use 24 bits, because that is what the ray tracing pipeline allows for custom ids
        // We reserve 4 for the instance table and 20 for the instance index
        u32 rtInstanceId;
    };

    struct draw_mesh_component
    {
        h32<draw_mesh> mesh;
    };

    struct draw_raytraced_tag
    {
    };
}