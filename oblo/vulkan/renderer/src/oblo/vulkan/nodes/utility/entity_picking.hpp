#pragma once

#include <oblo/core/thread/async_download.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo::vk
{
    struct picking_configuration;

    struct entity_picking
    {
        data<picking_configuration> inPickingConfiguration;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;
        resource<buffer> outPickingId;

        data<bool> outDummyOut;
        data<async_download> outPickingResult;

        h32<compute_pass> pickingPass;
        h32<compute_pass_instance> pickingPassInstance;
        h32<transfer_pass_instance> downloadInstance;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}