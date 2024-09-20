#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/instance_table_node.hpp>

namespace oblo::vk
{
    struct shadow_filter
    {
        resource<texture> inSource;

        resource<buffer> inCameraBuffer;

        //resource<buffer> inMeshDatabase;

        //resource<buffer> inInstanceTables;
        //data<instance_data_table_buffers_span> inInstanceBuffers;

        //resource<texture> inVisibilityBuffer;

        resource<texture> outFiltered;

        //resource<texture> stableHistory;
        //resource<texture> transientHistory;
        //resource<texture> historySamples;

        h32<compute_pass> filterPass;

        u32 passIndex;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}