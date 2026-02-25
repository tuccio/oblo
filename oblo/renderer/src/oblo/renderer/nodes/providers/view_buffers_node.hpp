#pragma once

#include <oblo/core/string/string_interner.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/data/camera_buffer.hpp>
#include <oblo/renderer/data/copy_texture_info.hpp>
#include <oblo/renderer/data/time_buffer.hpp>
#include <oblo/renderer/graph/frame_graph_context.hpp>
#include <oblo/renderer/graph/node_common.hpp>
#include <oblo/renderer/graph/pins.hpp>
#include <oblo/renderer/nodes/providers/instance_table_node.hpp>

namespace oblo
{
    struct view_buffers_node
    {
        pin::data<vec2u> inResolution;
        pin::data<camera_buffer> inCameraData;
        pin::data<time_buffer> inTimeData;
        pin::buffer inMeshDatabase;
        pin::buffer outCameraBuffer;
        pin::buffer outTimeBuffer;

        pin::buffer inInstanceTables;
        pin::data<instance_data_table_buffers_span> inInstanceBuffers;

        pin::data_sink<camera_buffer> outCameraDataSink;

        void build(const frame_graph_build_context& ctx)
        {
            const auto& cameraBuffer = ctx.access(inCameraData);
            const auto& timeBuffer = ctx.access(inTimeData);

            ctx.empty_pass();

            ctx.create(outCameraBuffer,
                {
                    .size = sizeof(camera_buffer),
                    .data = std::as_bytes(std::span{&cameraBuffer, 1}),
                },
                buffer_access::uniform);

            ctx.create(outTimeBuffer,
                {
                    .size = sizeof(time_buffer),
                    .data = std::as_bytes(std::span{&timeBuffer, 1}),
                },
                buffer_access::uniform);

            acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

            ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

            ctx.push(outCameraDataSink, ctx.access(inCameraData));
        }
    };
}