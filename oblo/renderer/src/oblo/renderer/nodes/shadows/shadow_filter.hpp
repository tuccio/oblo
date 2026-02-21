#pragma once

#include <oblo/core/types.hpp>
#include <oblo/renderer/data/raytraced_shadow_config.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

namespace oblo
{
    struct shadow_filter
    {
        pin::texture inSource;

        pin::buffer inCameraBuffer;

        pin::buffer inMeshDatabase;

        pin::buffer inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::texture inVisibilityBuffer;

        pin::texture outFiltered;

        data<raytraced_shadow_config> inConfig;

        h32<compute_pass> filterPass;
        h32<compute_pass_instance> filterPassInstance;

        u32 passIndex;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}