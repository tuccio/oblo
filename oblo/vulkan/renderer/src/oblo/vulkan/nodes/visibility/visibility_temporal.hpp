#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

namespace oblo::vk
{
    struct visibility_temporal
    {
        resource<buffer> inCameraBuffer;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;

        resource<texture> inCurrentDepth;
        resource<texture> inLastFrameDepth;

        h32<compute_pass> temporalPass;
        h32<compute_pass_instance> temporalPassInstance;

        resource<texture> outMotionVectors;
        resource<texture> outDisocclusionMask;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}