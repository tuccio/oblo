#include <oblo/graphics/systems/draw_registry_system.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/graphics/systems/system_metrics.hpp>
#include <oblo/metrics/metrics_collector.hpp>
#include <oblo/renderer/draw/draw_registry.hpp>
#include <oblo/renderer/renderer.hpp>

namespace oblo
{
    void draw_registry_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<renderer>();
        m_isRayTracingEnabled = m_renderer->is_ray_tracing_enabled();
        m_drawRegistry = ctx.services->find<draw_registry>();
        m_metrics = ctx.services->find<metrics_collector>();
        update(ctx);
    }

    void draw_registry_system::update(const ecs::system_update_context& ctx)
    {
        const hptr commandBuffer = m_renderer->get_active_command_buffer();
        m_drawRegistry->flush_uploads(commandBuffer);

        m_drawRegistry->generate_mesh_database(*ctx.frameAllocator);
        m_drawRegistry->generate_draw_calls(*ctx.frameAllocator);

        if (m_isRayTracingEnabled)
        {
            m_drawRegistry->generate_raytracing_structures(*ctx.frameAllocator, commandBuffer);
        }

        if (m_metrics && m_metrics->is_collecting())
        {
            draw_registry_metrics data{};

            const std::span batches = m_drawRegistry->get_draw_calls();

            for (const auto& b : batches)
            {
                data.totalInstances += b.numInstances;

                for (u32 i = 0; i < b.instanceBuffers.count; ++i)
                {
                    // As long as we updload everything, we can just sum the staging data up
                    for (const auto& segment : b.instanceBuffers.buffersData->segments)
                    {
                        data.totalInstanceDataSize += segment.end - segment.begin;
                    }
                }
            }

            m_metrics->push_data(data);
        }
    }
}