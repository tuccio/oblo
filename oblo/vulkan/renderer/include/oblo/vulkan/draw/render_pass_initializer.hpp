#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

#include <span>

#include <vulkan/vulkan.h>

namespace oblo::vk
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
        bool enable;
        VkBlendFactor srcColorBlendFactor;
        VkBlendFactor dstColorBlendFactor;
        VkBlendOp colorBlendOp;
        VkBlendFactor srcAlphaBlendFactor;
        VkBlendFactor dstAlphaBlendFactor;
        VkBlendOp alphaBlendOp;
        VkColorComponentFlags colorWriteMask;
    };

    struct render_pass_targets
    {
        buffered_array<VkFormat, 4> colorAttachmentFormats;
        VkFormat depthFormat{VK_FORMAT_UNDEFINED};
        VkFormat stencilFormat{VK_FORMAT_UNDEFINED};
        buffered_array<color_blend_attachment_state, 1> blendStates;
    };

    struct depth_stencil_state
    {
        VkPipelineDepthStencilStateCreateFlags flags;
        bool depthTestEnable;
        bool depthWriteEnable;
        VkCompareOp depthCompareOp;
        bool depthBoundsTestEnable;
        bool stencilTestEnable;
        VkStencilOpState front;
        VkStencilOpState back;
        f32 minDepthBounds;
        f32 maxDepthBounds;
    };

    struct rasterization_state
    {
        VkPipelineRasterizationStateCreateFlags flags;
        bool depthClampEnable;
        bool rasterizerDiscardEnable;
        VkPolygonMode polygonMode;
        VkCullModeFlags cullMode;
        VkFrontFace frontFace;
        bool depthBiasEnable;
        float depthBiasConstantFactor;
        float depthBiasClamp;
        float depthBiasSlopeFactor;
        float lineWidth;
    };

    struct render_pipeline_initializer
    {
        render_pass_targets renderTargets;
        depth_stencil_state depthStencilState;
        rasterization_state rasterizationState;
        VkPrimitiveTopology primitiveTopology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        std::span<const hashed_string_view> defines;
    };
}