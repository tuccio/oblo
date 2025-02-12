#pragma once

#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>
#include <oblo/vulkan/nodes/providers/render_world_provider.hpp>

namespace oblo::vk
{
    struct draw_buffer_data;

    struct draw_call_generator
    {
        h32<compute_pass> drawCallGeneratorPass;
        h32<compute_pass_instance> drawCallGeneratorPassInstance;

        data<std::span<draw_buffer_data>> inDrawBufferData;
        data<render_world> inRenderWorld;

        data<std::span<resource<buffer>>> outDrawCallBuffer;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<buffer> inMeshDatabase;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}