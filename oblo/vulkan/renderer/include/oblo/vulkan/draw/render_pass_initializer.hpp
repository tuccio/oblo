#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/string_interner.hpp>
#include <oblo/core/types.hpp>

#include <filesystem>
#include <span>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    enum class pipeline_stages : u8
    {
        vertex,
        fragment,
        enum_max
    };

    struct render_pass_stage
    {
        pipeline_stages stage;
        std::filesystem::path shaderSourcePath;
    };

    struct render_pass_initializer
    {
        std::string name;
        buffered_array<render_pass_stage, u8(pipeline_stages::enum_max)> stages;
    };

    struct render_pass_targets
    {
        buffered_array<VkFormat, 4> colorAttachmentFormats;
        VkFormat depthFormat{VK_FORMAT_UNDEFINED};
        VkFormat stencilFormat{VK_FORMAT_UNDEFINED};
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
        std::span<const h32<string>> defines;
    };
}