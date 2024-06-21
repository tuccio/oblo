#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/draw/buffer_binding_table.hpp>
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

    struct forward_pass
    {
        data<picking_configuration> inPickingConfiguration;

        data<vec2u> inResolution;
        data<buffer_binding_table> inPerViewBindingTable;
        data<std::span<draw_buffer_data>> inDrawData;
        data<std::span<resource<buffer>>> inDrawCallBuffer;

        resource<buffer> inLightData;
        resource<buffer> inLightConfig;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> outRenderTarget;
        resource<texture> outPickingIdBuffer;
        resource<texture> outDepthBuffer;

        h32<render_pass> renderPass;

        h32<string> pickingEnabledDefine;

        bool isPickingEnabled;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}