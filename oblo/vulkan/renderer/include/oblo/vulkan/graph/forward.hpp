#pragma once

#include <oblo/core/types.hpp>

namespace oblo::vk
{
    class frame_graph;

    class frame_graph_init_context;
    class frame_graph_build_context;
    class frame_graph_execute_context;

    struct compute_pass;
    struct render_pass;

    struct buffer;
    struct texture;

    enum class texture_usage : u8;
    enum class buffer_usage : u8;
    enum class pass_kind : u8;
}