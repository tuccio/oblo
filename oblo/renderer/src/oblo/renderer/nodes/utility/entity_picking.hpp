#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/async_download.hpp>
#include <oblo/renderer/data/picking_configuration.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo
{
    struct picking_configuration;

    struct entity_picking
    {
        pin::data<picking_configuration> inPickingConfiguration;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::texture inVisibilityBuffer;
        pin::buffer outPickingId;

        pin::data<bool> outDummyOut;
        pin::data<async_download> outPickingResult;

        h32<compute_pass> pickingPass;
        h32<compute_pass_instance> pickingPassInstance;
        h32<transfer_pass_instance> downloadInstance;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}