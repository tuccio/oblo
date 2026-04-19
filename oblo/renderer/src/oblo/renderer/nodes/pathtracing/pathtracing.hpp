#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

namespace oblo
{
    struct pathtracing
    {
        pin::data<vec2u> inResolution;

        pin::buffer inCameraBuffer;

        pin::buffer inLightBuffer;
        pin::buffer inLightConfig;

        pin::texture inMotionVectors;
        pin::texture inDisocclusionMask;

        pin::texture inGBuffer0;
        pin::texture inGBuffer1;
        pin::texture inGBuffer2;
        pin::texture inGBuffer3;

        pin::buffer inSkyboxSettingsBuffer;

        pin::texture samplesCountImage;
        pin::texture outShadedImage;

        h32<raytracing_pass> ptPass;
        h32<raytracing_pass_instance> ptPassInstance;
        h32<transfer_pass_instance> clearPassInstance;

        pin::buffer inMeshDatabase;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        u32 randomSeed;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}