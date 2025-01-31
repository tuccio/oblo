#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/iterator/deque_chunk_iterator.hpp>
#include <oblo/core/iterator/deque_chunk_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/vulkan/data/components.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/nodes/providers/ecs_entity_set_provider.hpp>

namespace oblo::vk
{
    namespace
    {
        struct ecs_entity_set_entry
        {
            ecs::entity entity;
            u32 globalInstanceId;
        };
    }

    void ecs_entity_set_provider::build(const frame_graph_build_context& ctx)
    {
        uploadPass = ctx.transfer_pass();

        ecs::entity_registry& reg = ctx.get_entity_registry();

        auto& frameAllocator = ctx.get_frame_allocator();

        deque<ecs_entity_set_entry> entities{&frameAllocator,
            deque_config{
                .elementsPerChunk = (1u << 14) / sizeof(ecs_entity_set_entry),
            }};

        // We always keep entity 0, we will store the number of entries in there
        entities.emplace_back();

        for (auto&& chunk : reg.range<const draw_instance_id_component>())
        {
            for (const auto [e, id] : chunk.zip<ecs::entity, draw_instance_id_component>())
            {
                const u32 entityIndex = reg.extract_entity_index(e);

                if (entityIndex >= entities.size())
                {
                    entities.resize(entityIndex + 1);
                }

                entities[entityIndex] = {
                    .entity = e,
                    .globalInstanceId = id.rtInstanceId,
                };
            }
        }

        entities[0].entity.value = u32(entities.size());

        const auto stagedSpans = allocate_n_span<staging_buffer_span>(frameAllocator, entities.chunks_count());
        auto nextStagedSpanIt = stagedSpans.data();

        for (const std::span chunk : deque_chunk_range(entities))
        {
            const auto stagedSpan = ctx.stage_upload(as_bytes(chunk));
            *nextStagedSpanIt = stagedSpan;
            ++nextStagedSpanIt;
        }

        ctx.create(outEntitySet,
            buffer_resource_initializer{
                .size = u32(sizeof(ecs_entity_set_entry) * entities.size()),
            },
            buffer_usage::storage_upload);

        stagedData = stagedSpans;
    }

    void ecs_entity_set_provider::execute(const frame_graph_execute_context& ctx)
    {
        if (!ctx.begin_pass(uploadPass))
        {
            return;
        }

        for (u32 offset = 0, index = 0; index < stagedData.size(); ++index)
        {
            const auto& stagedSpan = stagedData[index];

            ctx.upload(outEntitySet, stagedSpan, offset);

            const auto bytesCount = (stagedSpan.segments[0].end - stagedSpan.segments[0].begin) +
                (stagedSpan.segments[1].end - stagedSpan.segments[1].begin);

            offset += bytesCount;
        }

        ctx.end_pass();
    }
}