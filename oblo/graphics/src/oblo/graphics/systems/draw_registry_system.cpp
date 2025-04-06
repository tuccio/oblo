#include <oblo/graphics/systems/draw_registry_system.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo
{
    void draw_registry_system::first_update(const ecs::system_update_context& ctx)
    {
        auto* const renderer = ctx.services->find<vk::renderer>();
        m_isRayTracingEnabled = renderer->is_ray_tracing_enabled();
        m_vulkanContext = &renderer->get_vulkan_context();
        m_drawRegistry = ctx.services->find<vk::draw_registry>();
        update(ctx);
    }

    void draw_registry_system::update(const ecs::system_update_context& ctx)
    {
        auto&& commandBuffer = m_vulkanContext->get_active_command_buffer();
        m_drawRegistry->flush_uploads(commandBuffer);

        m_drawRegistry->generate_mesh_database(*ctx.frameAllocator);
        m_drawRegistry->generate_draw_calls(*ctx.frameAllocator);

        if (m_isRayTracingEnabled)
        {
            m_drawRegistry->generate_raytracing_structures(*ctx.frameAllocator, commandBuffer);
        }
    }
}