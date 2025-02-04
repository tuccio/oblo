#include <oblo/scene/systems/transform_system.hpp>

#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/reflection/fields.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/math/transform.hpp>
#include <oblo/scene/components/children_component.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/parent_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>

namespace oblo
{
    namespace
    {
        void update_global_transform(global_transform_component& globalTransform,
            const position_component* position,
            const rotation_component* rotation,
            const scale_component* scale,
            const global_transform_component* parent)
        {
            globalTransform.lastFrameLocalToWorld = globalTransform.localToWorld;

            globalTransform.localToWorld = make_transform_matrix(position ? position->value : vec3{},
                rotation ? rotation->value : quaternion::identity(),
                scale ? scale->value : vec3::splat(1.f));

            if (parent)
            {
                globalTransform.localToWorld = parent->localToWorld * globalTransform.localToWorld;
            }

            globalTransform.normalMatrix = transpose(inverse(globalTransform.localToWorld).value_or(mat4::identity()));
        }

        bool has_changes(const global_transform_component& lhs, const global_transform_component& rhs)
        {
            static_assert(!struct_has_padding<global_transform_component>());
            return std::memcmp(&lhs, &rhs, sizeof(global_transform_component)) != 0;
        }
    }

    void transform_system::update(const ecs::system_update_context& ctx)
    {
        ecs::entity_registry& reg = *ctx.entities;

        // We are interested in the modifications the last 2 frames, since we update the last frame transform here
        const auto currentModificationId = reg.get_modification_id();

        struct entity_stack_info
        {
            ecs::entity id;
            ecs::entity parent;
        };

        deque<entity_stack_info> stack;

        const u64 target = m_lastModificationId;

        // Update all the roots
        for (auto&& chunk : reg.range<global_transform_component>().notified(target))
        {
            std::span positions = chunk.try_get<const position_component>();
            std::span rotations = chunk.try_get<const rotation_component>();
            std::span scales = chunk.try_get<const scale_component>();

            bool anyChange = false;

            u32 entityIndex{};

            for (auto&& [e, globalTransform] : chunk.zip<ecs::entity, global_transform_component>())
            {
                auto* const position = positions.empty() ? nullptr : positions.data() + entityIndex;
                auto* const rotation = rotations.empty() ? nullptr : rotations.data() + entityIndex;
                auto* const scale = scales.empty() ? nullptr : scales.data() + entityIndex;
                ++entityIndex;

                auto* const parentComponent = reg.try_get<parent_component>(e);

                const global_transform_component* parentTransform =
                    parentComponent ? reg.try_get<global_transform_component>(parentComponent->parent) : nullptr;

                if (parentComponent && parentTransform && reg.is_notified(parentComponent->parent, target))
                {
                    // The parent has to be updated first
                    continue;
                }

                const auto oldTransform = globalTransform;

                update_global_transform(globalTransform, position, rotation, scale, parentTransform);

                if (has_changes(oldTransform, globalTransform))
                {
                    anyChange = true;
                }

                auto* const children = reg.try_get<children_component>(e);

                if (children)
                {
                    for (auto child : children->children)
                    {
                        stack.push_back({.id = child, .parent = e});
                    }
                }
            }

            if (anyChange)
            {
                chunk.notify();
            }
        }

        while (!stack.empty())
        {
            const auto [e, parent] = stack.back();
            stack.pop_back();

            auto* const globalTransform = reg.try_get<global_transform_component>(e);

            if (globalTransform)
            {
                auto* const position = reg.try_get<position_component>(e);
                auto* const rotation = reg.try_get<rotation_component>(e);
                auto* const scale = reg.try_get<scale_component>(e);

                const auto& parentTransform = reg.get<global_transform_component>(parent);

                const auto oldTransform = *globalTransform;

                update_global_transform(*globalTransform, position, rotation, scale, &parentTransform);

                if (has_changes(oldTransform, *globalTransform))
                {
                    reg.notify(e);
                }

                auto* const children = reg.try_get<children_component>(e);

                if (children)
                {
                    for (auto child : children->children)
                    {
                        stack.push_back({.id = child, .parent = e});
                    }
                }
            }
        }

        m_lastModificationId = currentModificationId;
    }
}