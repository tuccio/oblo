#pragma once

#include <oblo/core/string_interner.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/data/camera_buffer.hpp>
#include <oblo/vulkan/data/time_buffer.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/nodes/instance_table_node.hpp>

namespace oblo::vk
{
    struct view_buffers_node
    {
        data<vec2u> inResolution;
        data<camera_buffer> inCameraData;
        data<time_buffer> inTimeData;
        // TODO: This is terrible: it makes it hard to figure which buffers are being used where
        data<binding_table> outPerViewBindingTable;
        resource<buffer> outCameraBuffer;
        resource<buffer> outTimeBuffer;
        resource<buffer> outMeshDatabase;

        resource<buffer> inInstanceTables;
        data<instance_data_table_buffers_span> inInstanceBuffers;

        void build(const frame_graph_build_context& ctx)
        {
            const auto& cameraBuffer = ctx.access(inCameraData);
            const auto& timeBuffer = ctx.access(inTimeData);
            auto& drawRegistry = ctx.get_draw_registry();

            const std::span meshDatabaseData = drawRegistry.get_mesh_database_data();

            ctx.create(outCameraBuffer,
                {
                    .size = sizeof(camera_buffer),
                    .data = std::as_bytes(std::span{&cameraBuffer, 1}),
                },
                pass_kind::none,
                buffer_usage::uniform);

            ctx.create(outTimeBuffer,
                {
                    .size = sizeof(time_buffer),
                    .data = std::as_bytes(std::span{&timeBuffer, 1}),
                },
                pass_kind::none,
                buffer_usage::uniform);

            ctx.create(outMeshDatabase,
                {
                    .size = u32(meshDatabaseData.size()),
                    .data = meshDatabaseData,
                },
                pass_kind::none,
                buffer_usage::storage_read);

            acquire_instance_tables(ctx,
                inInstanceTables,
                inInstanceBuffers,
                pass_kind::none,
                buffer_usage::storage_read);
        }

        void execute(const frame_graph_execute_context& ctx)
        {
            // TODO (#31): Find a better place for these buffers
            auto* const perViewTable = &ctx.access(outPerViewBindingTable);
            perViewTable->clear();

            const buffer cameraBuffer = ctx.access(outCameraBuffer);
            const buffer meshDatabaseBuffer = ctx.access(outMeshDatabase);

            auto& interner = ctx.get_string_interner();

            const h32<string> cameraBufferName = interner.get_or_add("b_CameraBuffer");
            perViewTable->emplace(cameraBufferName, make_bindable_object(cameraBuffer));

            // TODO: This is not really per-view, but global
            const buffer timeBuffer = ctx.access(outTimeBuffer);
            const h32<string> timeBufferName = interner.get_or_add("b_TimeBuffer");
            perViewTable->emplace(timeBufferName, make_bindable_object(timeBuffer));

            const h32<string> meshTablesName = interner.get_or_add("b_MeshTables");
            perViewTable->erase(meshTablesName);

            if (meshDatabaseBuffer.buffer)
            {
                perViewTable->emplace(meshTablesName, make_bindable_object(meshDatabaseBuffer));
            }
        }
    };
}