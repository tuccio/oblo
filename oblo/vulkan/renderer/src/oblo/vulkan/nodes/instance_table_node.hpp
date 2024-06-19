#pragma once

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo::vk
{
    struct instance_data_table_buffers
    {
        std::span<resource<buffer>> bufferResources;
        const u32* bufferIds;
    };

    using instance_data_table_buffers_span = std::span<instance_data_table_buffers>;

    struct instance_data_table;

    struct instance_table_node
    {
        resource<buffer> outInstanceTables;
        data<instance_data_table_buffers_span> outInstanceBuffers;

        std::span<instance_data_table> instanceTableArray;

        void build(const frame_graph_build_context& ctx);
        void execute(const frame_graph_execute_context& ctx);
    };
}