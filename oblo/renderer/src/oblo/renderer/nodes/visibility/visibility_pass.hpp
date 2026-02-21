#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

#include <span>

namespace oblo
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
        resource<texture> outLastFrameDepthBuffer;
        resource<texture> outDepthBuffer;

        h32<render_pass> renderPass;
        h32<render_pass_instance> passInstance;
        h32<transfer_pass_instance> copyPassInstance;

        resource<texture> depthBuffer0;
        resource<texture> depthBuffer1;
        u8 outputIndex{0};

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}