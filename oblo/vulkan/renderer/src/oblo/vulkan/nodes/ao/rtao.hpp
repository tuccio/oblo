#pragma once

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

namespace oblo::vk
{
    struct rtao
    {
        resource<buffer> inCameraBuffer;

        resource<texture> inVisibilityBuffer;
        resource<texture> inOutHistory;
        resource<texture> outRTAmbientOcclusion;

        resource<texture> inDisocclusionMask;
        resource<texture> inMotionVectors;

        h32<raytracing_pass> rtPass;
        h32<raytracing_pass_instance> rtPassInstance;

        resource<buffer> inMeshDatabase;
        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        u32 randomSeed;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}