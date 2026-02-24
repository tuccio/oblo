#pragma once

#include <oblo/core/types.hpp>

namespace oblo::gpu
{
    struct acceleration_structure;
    struct bind_group_layout;
    struct bind_group;
    struct buffer_image_copy_descriptor;
    struct buffer_range;
    struct buffer;
    struct command_buffer_pool;
    struct command_buffer;
    struct compute_pipeline;
    struct device_address;
    struct fence;
    struct graphics_pass;
    struct graphics_pipeline;
    struct image_pool;
    struct image;
    struct native_window;
    struct queue;
    struct raytracing_pipeline;
    struct sampler;
    struct semaphore;
    struct shader_module;
    struct surface;
    struct swapchain;

    struct bind_group_descriptor;
    struct bind_group_layout_descriptor;
    struct buffer_copy_descriptor;
    struct buffer_descriptor;
    struct buffer_image_copy_descriptor;
    struct command_buffer_pool_descriptor;
    struct compute_pass;
    struct compute_pipeline_descriptor;
    struct device_descriptor;
    struct fence_descriptor;
    struct graphics_pass_descriptor;
    struct graphics_pipeline_descriptor;
    struct image_descriptor;
    struct instance_descriptor;
    struct memory_barrier_descriptor;
    struct present_descriptor;
    struct queue_submit_descriptor;
    struct raytracing_hit_group_descriptor;
    struct raytracing_pass;
    struct raytracing_pipeline_descriptor;
    struct sampler_descriptor;
    struct semaphore_descriptor;
    struct shader_module_descriptor;
    struct swapchain_descriptor;
    struct vertex_input_attribute_descriptor;
    struct vertex_input_binding_descriptor;

    struct bind_group_data;
    struct device_info;
    struct device_limits;
    struct global_memory_barrier;
    struct image_state_transition;
    struct memory_properties;
    struct push_constant_range;
    struct rectangle;

    class gpu_instance;
    class staging_buffer;

    struct staging_buffer_span;

    enum class error;

    enum class buffer_usage : u8;
    enum class data_format : u32;
    enum class image_resource_state : u8;
    enum class image_usage : u8;
    enum class maytracing_hit_type : u8;
    enum class resh_index_type : u8;
    enum class pass_kind : u8;
    enum class pipeline_sync_stage : u8;
    enum class resource_binding_kind : u8;
    enum class shader_module_format : u8;
    enum class shader_stage : u8;
    enum class vertex_input_rate : u8;

    using image_format = data_format;
}