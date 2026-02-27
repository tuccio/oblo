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
        pin::data<vec2u> inResolution;
        pin::data<std::span<draw_buffer_data>> inDrawData;
        pin::data<std::span<pin::buffer>> inDrawCallBuffer;

        pin::buffer inCameraBuffer;
        pin::buffer inMeshDatabase;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::texture outVisibilityBuffer;
        pin::texture outLastFrameDepthBuffer;
        pin::texture outDepthBuffer;

        h32<render_pass> renderPass;
        h32<render_pass_instance> passInstance;
        h32<transfer_pass_instance> copyPassInstance;

        pin::texture depthBuffer0;
        pin::texture depthBuffer1;
        u8 outputIndex{0};

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}