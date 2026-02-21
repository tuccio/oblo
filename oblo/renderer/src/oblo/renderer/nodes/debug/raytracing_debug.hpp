#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

namespace oblo
{
    struct raytracing_debug
    {
        data<vec2u> inResolution;

        resource<buffer> inCameraBuffer;

        resource<buffer> inLightBuffer;
        resource<buffer> inLightConfig;

        resource<buffer> inSkyboxSettingsBuffer;

        resource<texture> outShadedImage;

        h32<raytracing_pass> rtDebugPass;
        h32<raytracing_pass_instance> rtDebugPassInstance;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}