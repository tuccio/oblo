#pragma once

#include <oblo/core/types.hpp>

namespace oblo::gpu
{
    struct bindless_image;
    struct buffer;
    struct buffer_image_copy_descriptor;
    struct buffer_range;
    struct command_buffer_pool;
    struct command_buffer;
    struct device_address;
    struct fence;
    struct image;
    struct image_pool;
    struct native_window;
    struct queue;
    struct render_pass;
    struct render_pipeline;
    struct sampler;
    struct semaphore;
    struct shader_module;
    struct surface;
    struct swapchain;

    struct buffer_descriptor;
    struct buffer_copy_descriptor;
    struct buffer_image_copy_descriptor;
    struct command_buffer_pool_descriptor;
    struct device_descriptor;
    struct fence_descriptor;
    struct image_descriptor;
    struct instance_descriptor;
    struct present_descriptor;
    struct queue_submit_descriptor;
    struct semaphore_descriptor;
    struct sampler_descriptor;
    struct render_pass_descriptor;
    struct render_pipeline_descriptor;
    struct shader_module_descriptor;
    struct swapchain_descriptor;

    struct device_info;
    struct device_limits;
    struct memory_properties;

    class gpu_instance;
    class gpu_queue_context;
    class staging_buffer;

    struct staging_buffer_span;

    enum class error;

    enum class buffer_usage : u8;
    enum class image_usage : u8;
    enum class image_format : u32;
    enum class mesh_index_type : u8;
    enum class pass_kind : u8;
    enum class shader_stage : u8;
    enum class shader_module_format : u8;
}