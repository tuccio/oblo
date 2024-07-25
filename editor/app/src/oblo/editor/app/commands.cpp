#include <oblo/editor/app/commands.hpp>

#include <oblo/core/fixed_string.hpp>
#include <oblo/editor/services/registered_commands.hpp>
#include <oblo/graphics/components/light_component.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

#include <IconsFontAwesome6.h>

namespace oblo::editor
{
    namespace
    {
        template <typename T, typename Init, fixed_string Name>
        ecs::entity spawn_entity(ecs::entity_registry& reg)
        {
            constexpr char prefix[] = "New ";
            char buf[Name.size() + sizeof(prefix)];

            std::memcpy(buf, prefix, sizeof(prefix));
            std::memcpy(buf + sizeof(prefix) - 1, Name.data(), Name.size());

            buf[sizeof(prefix) + Name.size() - 1] = '\0';

            const auto e =
                ecs_utility::create_named_physical_entity<T>(reg, buf, vec3{}, quaternion{}, vec3::splat(1.f));

            Init{}(reg, e);

            return e;
        }

        template <typename T, typename Init, fixed_string Name>
        spawn_entity_command make_spawn_command(const char* icon)
        {
            return spawn_entity_command{
                command{
                    .icon = icon,
                    .name = Name.string,
                },
                &spawn_entity<T, Init, Name>,
            };
        }

        light_component& init_light_common(ecs::entity_registry& reg, ecs::entity e)
        {
            auto& light = reg.get<light_component>(e);

            light.color = vec3::splat(1.f);
            light.radius = 20.f;
            light.intensity = 5.f;

            light.isShadowCaster = false;
            light.hardShadows = false;
            light.shadowBias = .01f;
            light.shadowPunctualRadius = .15f;
            light.shadowSamples = 1;

            light.shadowTemporalAccumulationFactor = .3f;
            light.shadowBlurKernel = 3;
            light.shadowBlurSigma = 1.15f;

            light.spotInnerAngle = 30_deg;
            light.spotOuterAngle = 60_deg;

            return light;
        };
    }

    void fill_commands(registered_commands& commands)
    {
        using PointLightInit = decltype(
            [](ecs::entity_registry& reg, ecs::entity e) {
                auto& light = init_light_common(reg, e);
                light.type = light_type::point;
            }
        );

        using SpotLightInit = decltype(
            [](ecs::entity_registry& reg, ecs::entity e) {
                auto& light = init_light_common(reg, e);
                light.type = light_type::spot;
            }
        );

        using DirectionalLightInit = decltype(
            [](ecs::entity_registry& reg, ecs::entity e) {
                auto& light = init_light_common(reg, e);
                light.type = light_type::directional;
                light.intensity = 20.f;
                light.shadowSamples = 4;
                light.isShadowCaster = true;
            }
        );

        commands.spawnEntityCommands.push_back(
            make_spawn_command<light_component, PointLightInit, "Point Light">(ICON_FA_LIGHTBULB));

        commands.spawnEntityCommands.push_back(
            make_spawn_command<light_component, SpotLightInit, "Spot Light">(ICON_FA_VOLUME_OFF));

        commands.spawnEntityCommands.push_back(
            make_spawn_command<light_component, DirectionalLightInit, "Directional Light">(ICON_FA_SUN));
    }
}