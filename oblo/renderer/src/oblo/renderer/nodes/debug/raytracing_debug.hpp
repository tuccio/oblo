#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

namespace oblo
{
    struct raytracing_debug
    {
        data<vec2u> inResolution;

        pin::buffer inCameraBuffer;

        pin::buffer inLightBuffer;
        pin::buffer inLightConfig;

        pin::buffer inSkyboxSettingsBuffer;

        pin::texture outShadedImage;

        h32<raytracing_pass> rtDebugPass;
        h32<raytracing_pass_instance> rtDebugPassInstance;

        pin::buffer inMeshDatabase;

        pin::buffer inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}