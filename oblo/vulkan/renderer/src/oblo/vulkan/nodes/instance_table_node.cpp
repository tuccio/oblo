#include <oblo/vulkan/nodes/instance_table_node.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>

namespace oblo::vk
{
    constexpr u32 MaxInstanceBuffers{32};

    struct instance_data_table
    {
        u64 bufferAddress[MaxInstanceBuffers];
    };

    void instance_table_node::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::transfer);
        const auto& drawRegistry = ctx.get_draw_registry();

        const std::span meshDatabaseData = drawRegistry.get_mesh_database_data();

        ctx.create(outMeshDatabase,
            {
                .size = u32(meshDatabaseData.size()),
                .data = meshDatabaseData,
            },
            buffer_usage::storage_upload);

        const std::span drawCalls = drawRegistry.get_draw_calls();
        const auto numTables = drawCalls.size();

        auto& instanceBuffers = ctx.access(outInstanceBuffers);

        if (numTables == 0)
        {
            instanceBuffers = {};

            // Still create a dummy table, just not to upset anyone down the line
            ctx.create(outInstanceTables,
                buffer_resource_initializer{.size = sizeof(instance_data_table)},
                buffer_usage::storage_upload);

            return;
        }

        auto& frameAllocator = ctx.get_frame_allocator();

        instanceBuffers = allocate_n_span<instance_data_table_buffers>(frameAllocator, numTables);

        for (usize drawIndex = 0; drawIndex < drawCalls.size(); ++drawIndex)
        {
            const auto& srcInstanceBuffer = drawCalls[drawIndex].instanceBuffers;
            auto bufferResources = allocate_n_span<resource<buffer>>(frameAllocator, srcInstanceBuffer.count);

            for (u32 i = 0; i < srcInstanceBuffer.count; ++i)
            {
                bufferResources[i] =
                    ctx.create_dynamic_buffer(srcInstanceBuffer.buffersData[i], buffer_usage::storage_upload);
            }

            instanceBuffers[drawIndex] = {
                bufferResources,
                srcInstanceBuffer.instanceBufferIds,
            };
        }

        ctx.create(outInstanceTables,
            buffer_resource_initializer{.size = u32(numTables * sizeof(instance_data_table))},
            buffer_usage::storage_upload);

        instanceTableArray = allocate_n_span<instance_data_table>(frameAllocator, numTables);
    }

    void instance_table_node::execute(const frame_graph_execute_context& ctx)
    {
        auto& instanceBuffers = ctx.access(outInstanceBuffers);

        if (instanceBuffers.empty())
        {
            return;
        }

        for (usize i = 0; i < instanceBuffers.size(); ++i)
        {
            instanceTableArray[i] = {};

            for (usize j = 0; j < u32(instanceBuffers[i].bufferResources.size()); ++j)
            {
                const auto b = ctx.access(instanceBuffers[i].bufferResources[j]);

                const VkBufferDeviceAddressInfo info{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = b.buffer,
                };

                const auto id = instanceBuffers[i].bufferIds[j];
                instanceTableArray[i].bufferAddress[id] = vkGetBufferDeviceAddress(ctx.get_device(), &info) + b.offset;
            }
        }

        ctx.upload(outInstanceTables, as_bytes(instanceTableArray));
    }

    void acquire_instance_tables(const frame_graph_build_context& ctx,
        resource<buffer> instanceTables,
        data<instance_data_table_buffers_span> instanceBuffers,
        buffer_usage usage)
    {
        ctx.acquire(instanceTables, usage);

        for (const auto& instanceBuffer : ctx.access(instanceBuffers))
        {
            for (const auto& r : instanceBuffer.bufferResources)
            {
                ctx.acquire(r, usage);
            }
        }
    }
}