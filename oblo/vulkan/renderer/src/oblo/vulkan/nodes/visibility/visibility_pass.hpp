#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo::vk
{
    struct draw_buffer_data;

    struct visibility_pass
    {
        data<vec2u> inResolution;
        data<std::span<draw_buffer_data>> inDrawData;
        data<std::span<resource<buffer>>> inDrawCallBuffer;

        resource<buffer> inCameraBuffer;
        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> outVisibilityBuffer;
        resource<texture> outDepthBuffer;

        h32<render_pass> renderPass;

        void init(const frame_graph_init_context& context);

        void build(const frame_graph_build_context& builder);

        void execute(const frame_graph_execute_context& context);
    };
}