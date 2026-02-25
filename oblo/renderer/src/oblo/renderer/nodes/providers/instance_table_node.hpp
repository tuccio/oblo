#pragma once

#include <oblo/renderer/data/render_world.hpp>
#include <oblo/renderer/graph/enums.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

#include <span>

namespace oblo
{
    struct instance_data_table_buffers
    {
        std::span<pin::buffer> bufferResources;
        const u32* bufferIds;
    };

    using instance_data_table_buffers_span = std::span<instance_data_table_buffers>;

    struct instance_data_table;

    struct instance_table_node
    {
        pin::buffer outMeshDatabase;
        pin::buffer outInstanceTables;
        pin::data<instance_data_table_buffers_span> outInstanceBuffers;
        pin::data<render_world> inRenderWorld;

        std::span<instance_data_table> instanceTableArray;

        h32<transfer_pass_instance> uploadPass;

        void build(const frame_graph_build_context& ctx);
        void execute(const frame_graph_execute_context& ctx);
    };

    void acquire_instance_tables(const frame_graph_build_context& ctx,
        pin::buffer instanceTables,
        pin::data<instance_data_table_buffers_span> instanceBuffers,
        buffer_usage usage);
}