#pragma once

#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

namespace oblo
{
    struct rtao
    {
        pin::buffer inCameraBuffer;

        pin::texture inVisibilityBuffer;
        pin::texture inOutHistory;
        pin::texture inOutHistorySamplesCount;
        pin::texture outRTAmbientOcclusion;

        pin::texture inDisocclusionMask;
        pin::texture inMotionVectors;

        h32<raytracing_pass> rtPass;
        h32<raytracing_pass_instance> rtPassInstance;

        pin::buffer inMeshDatabase;
        pin::buffer inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        data<f32> inBias;
        data<f32> inMaxDistance;
        data<f32> inMaxHistoryWeight;

        u32 randomSeed;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}