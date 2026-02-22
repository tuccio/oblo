#pragma once

#include <oblo/core/forward.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    class frame_graph;
    struct frame_graph_impl;
    struct frame_graph_subgraph;

    struct frame_graph_build_args;
    struct frame_graph_execute_args;

    class frame_graph_init_context;
    class frame_graph_build_context;
    class frame_graph_execute_context;

    struct compute_pass;
    struct render_pass;
    struct raytracing_pass;

    struct frame_graph_pass;
    struct compute_pass_instance;
    struct raytracing_pass_instance;
    struct render_pass_instance;
    struct transfer_pass_instance;
    struct empty_pass_instance;

    struct compute_pass_initializer;
    struct render_pass_initializer;
    struct raytracing_pass_initializer;

    struct compute_pipeline_initializer;
    struct render_pipeline_initializer;
    struct raytracing_pipeline_initializer;

    struct render_pass_config;

    class binding_table;
    struct bindable_object;

    struct resident_texture;
    struct retained_texture;

    struct frame_graph_build_state;
    struct frame_graph_execution_state;
    struct frame_graph_pin_storage;

    struct frame_graph_buffer;
    struct frame_graph_texture;

    struct staging_buffer_span;

    enum class buffer_access : u8;
    enum class texture_access : u8;

    using async_download = future<dynamic_array<byte>>;
    using async_download_promise = promise<dynamic_array<byte>>;
}