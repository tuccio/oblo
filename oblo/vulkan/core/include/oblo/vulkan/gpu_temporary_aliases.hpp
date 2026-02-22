#pragma once

#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/vulkan/gpu_allocator.hpp>

namespace oblo::vk
{
    using gpu_allocator = gpu::vk::gpu_allocator;
    using allocated_buffer = gpu::vk::allocated_buffer;
    using allocated_image = gpu::vk::allocated_image;
    using buffer_initializer = gpu::vk::allocated_buffer_initializer;
    using image_initializer = gpu::vk::allocated_image_initializer;
    using memory_usage = gpu::vk::allocated_memory_usage;

    using gpu::texture_format;
    using gpu::texture_usage;

    using gpu::attachment_load_op;
    using gpu::attachment_store_op;
    using gpu::blend_factor;
    using gpu::blend_op;
    using gpu::color_component;
    using gpu::compare_op;
    using gpu::cull_mode;
    using gpu::front_face;
    using gpu::pipeline_depth_stencil_state_create;
    using gpu::polygon_mode;
    using gpu::primitive_topology;
    using gpu::stencil_op;
}