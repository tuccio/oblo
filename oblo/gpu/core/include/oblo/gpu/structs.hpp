#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/debug_label.hpp>
#include <oblo/core/variant.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/math/vec2i.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/vec3i.hpp>
#include <oblo/math/vec3u.hpp>

#include <optional>
#include <span>

namespace oblo::gpu
{
    enum class sampler_address_mode : u8
    {
        repeat,
        mirrored_repeat,
        clamp_to_edge,
        clamp_to_border,
        mirror_clamp_to_edge,
    };

    enum class sampler_filter : u8
    {
        nearest,
        linear,
    };

    enum class sampler_mipmap_mode : u8
    {
        nearest,
        linear,
    };

    struct sampler_descriptor
    {
        sampler_filter magFilter;
        sampler_filter minFilter;
        sampler_mipmap_mode mipmapMode;
        sampler_address_mode addressModeU;
        sampler_address_mode addressModeV;
        sampler_address_mode addressModeW;
        f32 mipLodBias;
        bool anisotropyEnable;
        f32 maxAnisotropy;
        f32 minLod;
        f32 maxLod;
        debug_label debugLabel;
    };

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

    struct memory_properties : variant<memory_usage, flags<memory_requirement>>
    {
    };

    struct push_constant_range
    {
        flags<shader_stage> stages;
        u32 size;
    };

    struct buffer_descriptor
    {
        u64 size;
        memory_properties memoryProperties;
        flags<buffer_usage> usages;
        debug_label debugLabel;
    };

    struct device_descriptor
    {
        bool requireHardwareRaytracing;
    };

    struct fence_descriptor
    {
        bool createSignaled;
        debug_label debugLabel;
    };

    struct image_descriptor
    {
        image_format format;
        u32 width;
        u32 height;
        u32 depth;
        u32 mipLevels;
        u32 arrayLayers;
        image_type type;
        samples_count samples;
        memory_usage memoryUsage;
        flags<image_usage> usages;
        debug_label debugLabel;
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
        const char* entryFunction;
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
        std::span<const image_format> colorAttachmentFormats;
        image_format depthFormat{image_format::undefined};
        image_format stencilFormat{image_format::undefined};
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
        std::span<const vertex_input_binding_descriptor> vertexInputBindings;
        std::span<const vertex_input_attribute_descriptor> vertexInputAttributes;
        std::span<const push_constant_range> pushConstants;
        std::span<const h32<bind_group_layout>> bindGroupLayouts;
        render_pass_targets renderTargets;
        depth_stencil_state depthStencilState;
        rasterization_state rasterizationState;
        primitive_topology primitiveTopology{primitive_topology::triangle_list};
        debug_label debugLabel;
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
        debug_label debugLabel;
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
        image_format format;
        u32 width;
        u32 height;
    };

    struct device_info
    {
        u32 subgroupSize;
        u64 optimalBufferCopyOffsetAlignment;
        u64 optimalBufferCopyRowPitchAlignment;
    };

    struct buffer_range
    {
        h32<buffer> buffer;
        u64 offset;
        u64 size;
    };

    struct image_state_transition
    {
        h32<image> image;
        image_resource_state previousState;
        image_resource_state nextState;
        pipeline_sync_stage previousPipeline;
        pipeline_sync_stage nextPipeline;
    };

    struct global_memory_barrier
    {
        flags<pipeline_sync_stage> previousPipelines;
        flags<pipeline_sync_stage> nextPipelines;
        flags<memory_access_type> previousAccesses;
        flags<memory_access_type> nextAccesses;
    };

    struct memory_barrier_descriptor
    {
        std::span<const image_state_transition> images;
        std::span<const global_memory_barrier> memory;
    };

    struct vertex_input_binding_descriptor
    {
        u32 binding;
        u32 stride;
        vertex_input_rate inputRate;
    };

    struct vertex_input_attribute_descriptor
    {
        u32 binding;
        u32 location;
        data_format format;
        u32 offset;
    };

    struct bind_group_binding
    {
        u32 binding;
        binding_kind bindingKind;
        flags<shader_stage> shaderStages;
        bool readOnly;
    };
    struct bind_group_layout_descriptor
    {
        std::span<const bind_group_binding> bindings;
    };
}