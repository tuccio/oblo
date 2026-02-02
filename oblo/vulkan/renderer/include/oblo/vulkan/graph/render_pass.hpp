#pragma once

#include <oblo/math/vec2i.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <optional>
#include <span>

namespace oblo::vk
{
    union clear_color_value {
        f32 f32[4];
        i32 i32[4];
        u32 u32[4];
    };

    struct clear_depth_stencil_value
    {
        f32 depth;
        u32 stencil;
    };

    union clear_value {
        clear_color_value color;
        clear_depth_stencil_value depthStencil;
    };

    struct render_attachment
    {
        resource<texture> texture;
        attachment_load_op loadOp;
        attachment_store_op storeOp;
        clear_value clearValue;
    };

    struct render_pass_config
    {
        vec2i renderOffset;
        vec2u renderResolution;

        std::span<const render_attachment> colorAttachments;
        std::optional<render_attachment> depthAttachment;
        std::optional<render_attachment> stencilAttachment;
    };
}