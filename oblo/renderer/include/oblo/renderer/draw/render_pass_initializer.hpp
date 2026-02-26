#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/structs.hpp>

#include <span>

namespace oblo
{
    struct render_pass_stage
    {
        gpu::shader_stage stage;
        string_view shaderSourcePath;
    };

    struct render_pass_initializer
    {
        string_view name;
        std::span<const render_pass_stage> stages;
    };

    struct render_pipeline_initializer
    {
        gpu::graphics_pass_targets renderTargets;
        gpu::depth_stencil_state depthStencilState;
        gpu::rasterization_state rasterizationState;
        gpu::primitive_topology primitiveTopology{gpu::primitive_topology::triangle_list};
        std::span<const hashed_string_view> defines;
    };
}