#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>

#include <span>

namespace oblo
{
    enum class raytracing_hit_type : u8
    {
        triangle,
        procedural,
    };

    struct raytracing_hit_group_initializer
    {
        raytracing_hit_type type;
        buffered_array<string_view, 2> shaders;
    };

    struct raytracing_pass_initializer
    {
        string_view name;
        string_view generation;
        buffered_array<string_view, 2> miss;
        buffered_array<raytracing_hit_group_initializer, 2> hitGroups;
    };

    struct raytracing_pipeline_initializer
    {
        u32 maxPipelineRayRecursionDepth{1};
        std::span<const hashed_string_view> defines;
    };
}