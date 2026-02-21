#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/variant.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/types.hpp>
#include <oblo/math/vec2i.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec3i.hpp>
#include <oblo/math/vec3u.hpp>

#include <optional>
#include <span>

namespace oblo::gpu
{
    struct command_buffer_pool_descriptor
    {
        h32<queue> queue;
        u32 numCommandBuffers;
    };

    struct buffer_copy_descriptor
    {
        u64 srcOffset;
        u64 dstOffset;
        u64 size;
    };

    struct image_subresource_descriptor
    {
        flags<image_aspect> aspectMask;
        u32 mipLevel;
        u32 baseArrayLayer;
        u32 layerCount;
    };

    struct buffer_image_copy_descriptor
    {
        u64 bufferOffset;
        u32 bufferRowLength;
        u32 bufferImageHeight;
        image_subresource_descriptor imageSubresource;
        vec3i imageOffset;
        vec3u imageExtent;
    };

    struct buffer_descriptor
    {
        u64 size;

        variant<memory_usage, flags<memory_requirement>> memoryFlags;

        flags<buffer_usage> usages;
    };

    struct device_descriptor
    {
        bool requireHardwareRaytracing;
    };

    struct fence_descriptor
    {
        bool createSignaled;
    };

    struct image_descriptor
    {
        texture_format format;
        u32 width;
        u32 height;
        u32 depth;
        u32 mipLevels;
        u32 arrayLayers;
        image_type type;
        samples_count samples;
        memory_usage memoryUsage;
        flags<texture_usage> usages;
    };

    struct instance_descriptor
    {
        const char* application;
        const char* engine;
    };

    struct present_descriptor
    {
        std::span<const h32<swapchain>> swapchains;
        std::span<const h32<semaphore>> waitSemaphores;
    };

    struct queue_submit_descriptor
    {
        std::span<const hptr<command_buffer>> commandBuffers;
        std::span<const h32<semaphore>> waitSemaphores;
        h32<fence> signalFence;
        std::span<const h32<semaphore>> signalSemaphores;
    };

    struct render_pipeline_stage
    {
        shader_stage stage;
        h32<shader_module> shaderModule;
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
        std::span<const texture_format> colorAttachmentFormats;
        texture_format depthFormat{texture_format::undefined};
        texture_format stencilFormat{texture_format::undefined};
        std::span<const color_blend_attachment_state> blendStates;
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

    struct render_pipeline_descriptor
    {
        std::span<const render_pipeline_stage> stages;
        render_pass_targets renderTargets;
        depth_stencil_state depthStencilState;
        rasterization_state rasterizationState;
        primitive_topology primitiveTopology{primitive_topology::triangle_list};
    };

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
        h32<image> image;
        attachment_load_op loadOp;
        attachment_store_op storeOp;
        clear_value clearValue;
    };

    struct render_pass_descriptor
    {
        vec2i renderOffset;
        vec2u renderResolution;

        std::span<const render_attachment> colorAttachments;
        std::optional<render_attachment> depthAttachment;
        std::optional<render_attachment> stencilAttachment;
    };

    struct semaphore_descriptor
    {
        bool timeline;
        u64 timelineInitialValue;
    };

    struct shader_module_descriptor
    {
        shader_module_format format;
        std::span<const u8> data;
    };

    struct swapchain_descriptor
    {
        hptr<surface> surface;
        u32 numImages;
        texture_format format;
        u32 width;
        u32 height;
    };

    struct device_info
    {
        u32 subgroupSize;
        u64 optimalBufferCopyOffsetAlignment;
        u64 optimalBufferCopyRowPitchAlignment;
    };
}