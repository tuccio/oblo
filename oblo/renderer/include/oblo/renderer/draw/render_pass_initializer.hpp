#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/enums.hpp>

#include <span>

namespace oblo
{
    enum class pipeline_stages : u8
    {
        mesh,
        vertex,
        fragment,
        enum_max
    };

    struct render_pass_stage
    {
        pipeline_stages stage;
        string_view shaderSourcePath;
    };

    struct render_pass_initializer
    {
        string_view name;
        buffered_array<render_pass_stage, u8(pipeline_stages::enum_max)> stages;
    };

    struct color_blend_attachment_state
    {
        bool enable = false;
        blend_factor srcColorBlendFactor;
        blend_factor dstColorBlendFactor;
        blend_op colorBlendOp;
        blend_factor srcAlphaBlendFactor;
        blend_factor dstAlphaBlendFactor;
        blend_op alphaBlendOp;
        flags<color_component> colorWriteMask =
            color_component::r | color_component::g | color_component::b | color_component::a;
    };

    struct render_pass_targets
    {
        buffered_array<texture_format, 4> colorAttachmentFormats;
        texture_format depthFormat{texture_format::undefined};
        texture_format stencilFormat{texture_format::undefined};
        buffered_array<color_blend_attachment_state, 1> blendStates;
    };

    struct stencil_op_state
    {
        stencil_op failOp;
        stencil_op passOp;
        stencil_op depthFailOp;
        compare_op compareOp;
        u32 compareMask;
        u32 writeMask;
        u32 reference;
    };

    struct depth_stencil_state
    {
        flags<pipeline_depth_stencil_state_create> flags;
        bool depthTestEnable;
        bool depthWriteEnable;
        compare_op depthCompareOp;
        bool depthBoundsTestEnable;
        bool stencilTestEnable;
        stencil_op_state front;
        stencil_op_state back;
        f32 minDepthBounds;
        f32 maxDepthBounds;
    };

    struct rasterization_state
    {
        flags<pipeline_depth_stencil_state_create> flags;
        bool depthClampEnable;
        bool rasterizerDiscardEnable;
        polygon_mode polygonMode;
        oblo::flags<cull_mode> cullMode;
        front_face frontFace;
        bool depthBiasEnable;
        f32 depthBiasConstantFactor;
        f32 depthBiasClamp;
        f32 depthBiasSlopeFactor;
        f32 lineWidth;
    };

    struct render_pipeline_initializer
    {
        render_pass_targets renderTargets;
        depth_stencil_state depthStencilState;
        rasterization_state rasterizationState;
        primitive_topology primitiveTopology{primitive_topology::triangle_list};
        std::span<const hashed_string_view> defines;
    };
}