#include <oblo/graphics/systems/draw_registry_system.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/renderer/draw/draw_registry.hpp>
#include <oblo/renderer/renderer.hpp>

namespace oblo
{
    void draw_registry_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<renderer>();
        m_isRayTracingEnabled = m_renderer->is_ray_tracing_enabled();
        m_drawRegistry = ctx.services->find<draw_registry>();
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
    }
}