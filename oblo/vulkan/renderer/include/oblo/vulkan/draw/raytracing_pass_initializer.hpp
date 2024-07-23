#pragma once

#include <oblo/core/buffered_array.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

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
        buffered_array<std::filesystem::path, 2> shaders;
    };

    struct raytracing_pass_initializer
    {
        std::string name;
        std::filesystem::path generation;
        std::filesystem::path miss;
        buffered_array<raytracing_hit_group_initializer, 2> hitGroups;
    };

    struct raytracing_pipeline_initializer
    {
        u32 maxPipelineRayRecursionDepth{1};
        std::span<const std::string_view> defines;
    };
}