#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

namespace oblo::vk
{
    struct visibility_extra_buffers
    {
        enum class enabled_buffers : u8
        {
            motion_vectors,
            disocclusion_mask,
            enum_max,
        };

        resource<buffer> inCameraBuffer;

        resource<buffer> inMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        resource<texture> inVisibilityBuffer;

        resource<texture> inCurrentDepth;
        resource<texture> inLastFrameDepth;

        h32<compute_pass> extraBuffersPass;
        h32<compute_pass_instance> extraBuffersPassInstance;

        resource<texture> outMotionVectors;
        resource<texture> outDisocclusionMask;

        flags<enabled_buffers> enabledBuffers{};

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}