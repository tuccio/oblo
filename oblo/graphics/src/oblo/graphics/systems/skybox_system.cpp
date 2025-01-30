#include <oblo/graphics/systems/skybox_system.hpp>

#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/graphics/components/skybox_component.hpp>
#include <oblo/graphics/systems/scene_renderer.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/vulkan/data/skybox_settings.hpp>

namespace oblo
{
    namespace
    {
        const skybox_component* find_skybox(ecs::entity_registry& reg)
        {
            for (auto&& chunk : reg.range<const skybox_component>())
            {
                for (const auto& skybox : chunk.get<const skybox_component>())
                {
                    if (skybox.texture)
                    {
                        return &skybox;
                    }
                }
            }

            return nullptr;
        }
    }

    void skybox_system::first_update(const ecs::system_update_context& ctx)
    {
        m_sceneRenderer = ctx.services->find<scene_renderer>();
        OBLO_ASSERT(m_sceneRenderer);

        m_resourceRegistry = ctx.services->find<resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        update(ctx);
    }

    void skybox_system::update(const ecs::system_update_context& ctx)
    {
        const auto* skybox = find_skybox(*ctx.entities);

        if (skybox)
        {
            const auto skyboxPtr = m_resourceRegistry->get_resource(skybox->texture.id).as<texture>();

            m_sceneRenderer->setup_skybox(skyboxPtr,
                {
                    .multiplier = skybox->tint * skybox->multiplier,
                });
        }
    }
}