#pragma once

#include <oblo/core/string_interner.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/data/camera_buffer.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/runtime_builder.hpp>
#include <oblo/vulkan/graph/runtime_context.hpp>

namespace oblo::vk
{
    struct view_buffers_node
    {
        data<camera_buffer> inCameraData;
        data<buffer_binding_table> outPerViewBindingTable;
        resource<buffer> outViewBuffer;

        void build(const runtime_builder& builder)
        {
            const auto& cameraBuffer = builder.access(inCameraData);

            builder.create(outViewBuffer,
                {
                    .size = sizeof(camera_buffer),
                    .data = std::as_bytes(std::span{&cameraBuffer, 1}),
                },
                buffer_usage::uniform);
        }

        void execute(const runtime_context& context)
        {
            auto* const perViewTable = context.access(outPerViewBindingTable);
            const buffer buf = context.access(outViewBuffer);

            auto& interner = context.get_string_interner();

            const h32<string> cameraBufferName = interner.get_or_add("b_CameraBuffer");
            perViewTable->emplace(cameraBufferName, buf);

            // TODO: Find a better place for this, maybe consider per frame descriptor sets
            const auto meshDbBuffer = context.get_draw_registry().get_mesh_database_buffer();

            const h32<string> meshTablesName = interner.get_or_add("b_MeshTables");
            perViewTable->erase(meshTablesName);

            if (meshDbBuffer.buffer)
            {
                perViewTable->emplace(meshTablesName, meshDbBuffer);
            }
        }
    };
}