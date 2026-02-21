#pragma once

#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/draw/draw_registry.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>
#include <oblo/renderer/nodes/providers/render_world_provider.hpp>

namespace oblo
{
    struct draw_buffer_data;

    struct frustum_culling
    {
        h32<compute_pass> cullPass;
        h32<compute_pass_instance> cullPassInstance;

        data<std::span<draw_buffer_data>> outDrawBufferData;

        resource<buffer> inMeshDatabase;
        resource<buffer> inCameraBuffer;
        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;
        data<render_world> inRenderWorld;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}