#pragma once

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo::vk
{
    struct instance_data_table_buffers;
    struct instance_data_table;

    struct instance_table_node
    {
        resource<buffer> outInstanceTables;

        std::span<instance_data_table_buffers> instanceBuffers;
        std::span<instance_data_table> instanceTableArray;

        void build(const frame_graph_build_context& ctx);
        void execute(const frame_graph_execute_context& ctx);
    };
}