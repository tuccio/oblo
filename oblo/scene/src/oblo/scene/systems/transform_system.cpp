#include <oblo/scene/systems/transform_system.hpp>

#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/math/transform.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>

namespace oblo
{
    void transform_system::update(const ecs::system_update_context& ctx)
    {
        for (const auto [entities, globalTransforms] : ctx.entities->range<global_transform_component>())
        {
            for (auto&& [e, globalTransform] : zip_range(entities, globalTransforms))
            {
                auto* const position = ctx.entities->try_get<position_component>(e);
                auto* const rotation = ctx.entities->try_get<rotation_component>(e);
                auto* const scale = ctx.entities->try_get<scale_component>(e);

                globalTransform.value = make_transform_matrix(position ? position->value : vec3{},
                    rotation ? rotation->value : quaternion::identity(),
                    scale ? scale->value : vec3::splat(1.f));
            }
        }
    }
}