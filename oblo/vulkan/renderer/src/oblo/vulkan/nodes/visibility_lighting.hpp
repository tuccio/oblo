#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/instance_table_node.hpp>

#include <span>

namespace oblo
{
    struct string;
}

namespace oblo::vk
{
    struct draw_buffer_data;
    struct picking_configuration;

    struct visibility_lighting
    {
        data<vec2u> inResolution;

        resource<buffer> inLightData;
        resource<buffer> inLightConfig;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;
        resource<texture> outLitImage;

        h32<compute_pass> lightingPass;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}