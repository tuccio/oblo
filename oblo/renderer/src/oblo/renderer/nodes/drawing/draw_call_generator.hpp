#pragma once

#include <oblo/renderer/draw/binding_table.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>
#include <oblo/renderer/nodes/providers/render_world_provider.hpp>

namespace oblo
{
    struct draw_buffer_data;

    struct draw_call_generator
    {
        h32<compute_pass> drawCallGeneratorPass;
        h32<compute_pass_instance> drawCallGeneratorPassInstance;

        data<std::span<draw_buffer_data>> inDrawBufferData;
        data<render_world> inRenderWorld;

        data<std::span<pin::buffer>> outDrawCallBuffer;

        pin::buffer inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::buffer inMeshDatabase;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}