#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo
{
    class string;
}

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

        h32<compute_pass> pickingPass;
        h32<compute_pass_instance> pickingPassInstance;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}