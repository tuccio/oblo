#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/data/raytraced_shadow_config.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

namespace oblo::vk
{
    struct shadow_temporal
    {
        resource<texture> inShadow;
        resource<texture> inShadowMean;
        resource<texture> inHistory;
        resource<texture> inOutHistorySamplesCount;

        resource<buffer> inCameraBuffer;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;

        resource<texture> outFiltered;
        resource<texture> outShadowMoments;

        h32<compute_pass> temporalPass;
        h32<compute_pass_instance> temporalPassInstance;

        data<raytraced_shadow_config> inConfig;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}