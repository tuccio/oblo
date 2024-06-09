#pragma once

#include <oblo/core/string_interner.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/data/camera_buffer.hpp>
#include <oblo/vulkan/data/time_buffer.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>

namespace oblo::vk
{
    struct view_buffers_node
    {
        data<camera_buffer> inCameraData;
        data<time_buffer> inTimeData;
        // TODO: This is terrible: it makes it hard to figure which buffers are being used where
        data<buffer_binding_table> outPerViewBindingTable;
        resource<buffer> outCameraBuffer;
        resource<buffer> outTimeBuffer;
        resource<buffer> outMeshDatabase;

        void build(const runtime_builder& builder)
        {
            const auto& cameraBuffer = builder.access(inCameraData);
            const auto& timeBuffer = builder.access(inTimeData);
            auto& drawRegistry = builder.get_draw_registry();

            const std::span meshDatabaseData = drawRegistry.get_mesh_database_data();

            builder.create(outCameraBuffer,
                {
                    .size = sizeof(camera_buffer),
                    .data = std::as_bytes(std::span{&cameraBuffer, 1}),
                },
                pass_kind::graphics, // TODO: Actually not used
                buffer_usage::uniform);

            builder.create(outTimeBuffer,
                {
                    .size = sizeof(time_buffer),
                    .data = std::as_bytes(std::span{&timeBuffer, 1}),
                },
                pass_kind::graphics, // TODO: Actually not used
                buffer_usage::uniform);

            if (!meshDatabaseData.empty())
            {
                builder.create(outMeshDatabase,
                    {
                        .size = u32(meshDatabaseData.size()),
                        .data = meshDatabaseData,
                    },
                    pass_kind::compute, // TODO: Actually not used
                    buffer_usage::storage_read);
            }
        }

        void execute(const runtime_context& context)
        {
            // TODO (#31): Find a better place for these buffers
            auto* const perViewTable = context.access(outPerViewBindingTable);
            const buffer cameraBuffer = context.access(outCameraBuffer);
            const buffer meshDatabaseBuffer = context.access(outMeshDatabase);

            auto& interner = context.get_string_interner();

            const h32<string> cameraBufferName = interner.get_or_add("b_CameraBuffer");
            perViewTable->emplace(cameraBufferName, cameraBuffer);

            // TODO: This is not really per-view, but global
            const buffer timeBuffer = context.access(outTimeBuffer);
            const h32<string> timeBufferName = interner.get_or_add("b_TimeBuffer");
            perViewTable->emplace(timeBufferName, timeBuffer);

            const h32<string> meshTablesName = interner.get_or_add("b_MeshTables");
            perViewTable->erase(meshTablesName);

            if (meshDatabaseBuffer.buffer)
            {
                perViewTable->emplace(meshTablesName, meshDatabaseBuffer);
            }
        }

        void build(const frame_graph_build_context& builder)
        {
            const auto& cameraBuffer = builder.access(inCameraData);
            const auto& timeBuffer = builder.access(inTimeData);
            auto& drawRegistry = builder.get_draw_registry();

            const std::span meshDatabaseData = drawRegistry.get_mesh_database_data();

            builder.create(outCameraBuffer,
                {
                    .size = sizeof(camera_buffer),
                    .data = std::as_bytes(std::span{&cameraBuffer, 1}),
                },
                pass_kind::graphics, // TODO: Actually not used
                buffer_usage::uniform);

            builder.create(outTimeBuffer,
                {
                    .size = sizeof(time_buffer),
                    .data = std::as_bytes(std::span{&timeBuffer, 1}),
                },
                pass_kind::graphics, // TODO: Actually not used
                buffer_usage::uniform);

            if (!meshDatabaseData.empty())
            {
                builder.create(outMeshDatabase,
                    {
                        .size = u32(meshDatabaseData.size()),
                        .data = meshDatabaseData,
                    },
                    pass_kind::compute, // TODO: Actually not used
                    buffer_usage::storage_read);
            }
        }

        void execute(const frame_graph_execute_context& context)
        {
            // TODO (#31): Find a better place for these buffers
            auto* const perViewTable = &context.access(outPerViewBindingTable);
            const buffer cameraBuffer = context.access(outCameraBuffer);
            const buffer meshDatabaseBuffer = context.access(outMeshDatabase);

            auto& interner = context.get_string_interner();

            const h32<string> cameraBufferName = interner.get_or_add("b_CameraBuffer");
            perViewTable->emplace(cameraBufferName, cameraBuffer);

            // TODO: This is not really per-view, but global
            const buffer timeBuffer = context.access(outTimeBuffer);
            const h32<string> timeBufferName = interner.get_or_add("b_TimeBuffer");
            perViewTable->emplace(timeBufferName, timeBuffer);

            const h32<string> meshTablesName = interner.get_or_add("b_MeshTables");
            perViewTable->erase(meshTablesName);

            if (meshDatabaseBuffer.buffer)
            {
                perViewTable->emplace(meshTablesName, meshDatabaseBuffer);
            }
        }
    };
}