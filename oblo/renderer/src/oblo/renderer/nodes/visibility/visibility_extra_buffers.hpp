#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/types.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

namespace oblo
{
    struct visibility_extra_buffers
    {
        enum class enabled_buffers : u8
        {
            motion_vectors,
            disocclusion_mask,
            enum_max,
        };

        pin::buffer inCameraBuffer;

        pin::buffer inMeshDatabase;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::texture inVisibilityBuffer;

        pin::texture inCurrentDepth;
        pin::texture inLastFrameDepth;

        h32<compute_pass> extraBuffersPass;
        h32<compute_pass_instance> extraBuffersPassInstance;

        pin::texture outMotionVectors;
        pin::texture outDisocclusionMask;

        flags<enabled_buffers> enabledBuffers{};

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}