#pragma once

#include <oblo/core/string/string_interner.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/data/camera_buffer.hpp>
#include <oblo/vulkan/data/copy_texture_info.hpp>
#include <oblo/vulkan/data/time_buffer.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/providers/instance_table_node.hpp>

namespace oblo::vk
{
    struct view_buffers_node
    {
        data<vec2u> inResolution;
        data<camera_buffer> inCameraData;
        data<time_buffer> inTimeData;
        resource<buffer> inMeshDatabase;
        resource<buffer> outCameraBuffer;
        resource<buffer> outTimeBuffer;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        data<copy_texture_info> inFinalRenderTarget;

        data_sink<camera_buffer> outCameraDataSink;

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
                buffer_usage::uniform);

            ctx.create(outTimeBuffer,
                {
                    .size = sizeof(time_buffer),
                    .data = std::as_bytes(std::span{&timeBuffer, 1}),
                },
                buffer_usage::uniform);

            acquire_instance_tables(ctx, inInstanceTables, inInstanceBuffers, buffer_usage::storage_read);

            ctx.acquire(inMeshDatabase, buffer_usage::storage_read);

            ctx.push(outCameraDataSink, ctx.access(inCameraData));
        }
    };
}