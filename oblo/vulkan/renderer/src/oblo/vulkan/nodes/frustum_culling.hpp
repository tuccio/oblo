#pragma once

#include <oblo/vulkan/draw/buffer_binding_table.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/instance_table_node.hpp>

namespace oblo::vk
{
    struct draw_buffer_data;
    struct frustum_culling_data;

    struct frustum_culling
    {
        h32<compute_pass> cullPass;
        h32<string> drawIndexedDefine;

        data<std::span<frustum_culling_data>> cullInternalData;

        data<buffer_binding_table> inPerViewBindingTable;

        data<std::span<draw_buffer_data>> outDrawBufferData;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}