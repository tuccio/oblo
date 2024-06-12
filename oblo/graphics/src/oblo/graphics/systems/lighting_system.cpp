#include <oblo/graphics/systems/lighting_system.hpp>

#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/graphics/components/light_component.hpp>
#include <oblo/graphics/systems/scene_renderer.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/vulkan/data/light_data.hpp>

namespace oblo
{
    void lighting_system::first_update(const ecs::system_update_context& ctx)
    {
        m_sceneRenderer = ctx.services->find<scene_renderer>();
        OBLO_ASSERT(m_sceneRenderer);

        m_sceneRenderer->ensure_setup();

        // Hacky setup for directional light
        const auto e = ecs_utility::create_named_physical_entity<light_component>(*ctx.entities,
            "Sun",
            {},
            quaternion::from_euler_xyz_intrinsic(degrees_tag{}, vec3{.x = -53.f, .y = -8.f, .z = -32.f}),
            vec3::splat(1.f));

        ctx.entities->get<light_component>(e) = {
            .type = light_type::directional,
            .color = vec3::splat(1.f),
            .intensity = 3.f,
        };

        update(ctx);
    }

    void lighting_system::update(const ecs::system_update_context& ctx)
    {
        const auto lightsRange = ctx.entities->range<light_component, global_transform_component>();

        dynamic_array<vk::light_data> lightData{ctx.frameAllocator};
        lightData.reserve(lightsRange.count());

        for (const auto [entities, lights, transforms] : lightsRange)
        {
            for (const auto& [light, transform] : zip_range(lights, transforms))
            {
                const vec4 position = transform.value.columns[3];
                const vec4 direction = normalize(transform.value * vec4{.z = -1.f});

                const f32 cosInner = std::cos(light.spotInnerAngle.value);
                const f32 cosOuter = std::cos(light.spotOuterAngle.value);

                f32 angleScale{0.f};
                f32 angleOffset{1.f};

                if (light.type == light_type::spot)
                {
                    angleScale = 1.f / max(.001f, cosInner - cosOuter);
                    angleOffset = -cosOuter * angleScale;
                }

                lightData.push_back({
                    .position = {position.x, position.y, position.z},
                    .invSqrRadius = 1.f / (light.radius * light.radius),
                    .direction = {direction.x, direction.y, direction.z},
                    .type = light.type,
                    .intensity = light.color * light.intensity,
                    .lightAngleScale = angleScale,
                    .lightAngleOffset = angleOffset,
                });
            }
        }

        m_sceneRenderer->set_light_data(lightData);
    }
}