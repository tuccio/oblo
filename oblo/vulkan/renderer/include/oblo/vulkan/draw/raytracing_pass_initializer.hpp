#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_view.hpp>

#include <span>

namespace oblo::vk
{
    enum class raytracing_hit_type : u8
    {
        triangle,
        procedural,
    };

    struct raytracing_hit_group_initializer
    {
        raytracing_hit_type type;
        buffered_array<string, 2> shaders;
    };

    struct raytracing_pass_initializer
    {
        string name;
        string generation;
        string miss;
        buffered_array<raytracing_hit_group_initializer, 2> hitGroups;
    };

    struct raytracing_pipeline_initializer
    {
        u32 maxPipelineRayRecursionDepth{1};
        std::span<const string_view> defines;
    };
}