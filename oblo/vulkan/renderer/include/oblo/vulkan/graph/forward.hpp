#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    class frame_graph;
    struct frame_graph_subgraph;

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

    struct acceleration_structure;
    struct buffer;
    struct texture;

    enum class texture_usage : u8;
    enum class buffer_usage : u8;
    enum class pass_kind : u8;
    enum class shader_stage : u8;
}