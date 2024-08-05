#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    enum class visibility_debug_mode : u8
    {
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
    };
}