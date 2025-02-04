#include <oblo/scene/utility/ecs_utility.hpp>

#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/transform.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/reflection/concepts/ranged_type_erasure.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/scene/components/children_component.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/scene/components/name_component.hpp>
#include <oblo/scene/components/parent_component.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
    struct tag_type_tag;
}

namespace oblo::ecs_utility
{
    namespace
    {
        void register_reflected_component_types(const reflection::reflection_registry& reflection,
            ecs::type_registry* typeRegistry,
            property_registry* propertyRegistry,
            dynamic_array<reflection::type_handle>& componentTypes)
        {
            componentTypes.clear();
            reflection.find_by_tag<ecs::component_type_tag>(componentTypes);

            if (typeRegistry)
            {
                for (const auto typeHandle : componentTypes)
                {
                    const auto typeData = reflection.get_type_data(typeHandle);

                    if (typeRegistry->find_component(typeData.type))
                    {
                        continue;
                    }

                    const auto rte = reflection.find_concept<reflection::ranged_type_erasure>(typeHandle);

                    if (rte)
                    {
                        const ecs::component_type_desc desc{
                            .type = typeData.type,
                            .size = typeData.size,
                            .alignment = typeData.alignment,
                            .create = rte->create,
                            .destroy = rte->destroy,
                            .move = rte->move,
                            .moveAssign = rte->moveAssign,
                        };

                        if (!typeRegistry->register_component(desc))
                        {
                            log::error("Failed to register component {}", typeData.type.name);
                        }
                    }
                }
            }

            if (propertyRegistry)
            {
                for (const auto typeHandle : componentTypes)
                {
                    const auto typeData = reflection.get_type_data(typeHandle);
                    propertyRegistry->build_from_reflection(typeData.type);
                }
            }
        }

        void register_reflected_tag_types(const reflection::reflection_registry& reflection,
            ecs::type_registry* typeRegistry,
            dynamic_array<reflection::type_handle>& tagTypes)
        {
            tagTypes.clear();
            reflection.find_by_tag<ecs::tag_type_tag>(tagTypes);

            if (typeRegistry)
            {
                for (const auto typeHandle : tagTypes)
                {
                    const auto typeData = reflection.get_type_data(typeHandle);

                    if (typeRegistry->find_tag(typeData.type))
                    {
                        continue;
                    }

                    const ecs::tag_type_desc desc{
                        .type = typeData.type,
                    };

                    if (!typeRegistry->register_tag(desc))
                    {
                        log::error("Failed to register tag {}", typeData.type.name);
                    }
                }
            }
        }

        void attach_root_entity_to_parent(ecs::entity_registry& registry, ecs::entity e, ecs::entity parent)
        {
            {
                auto& entityParent = registry.add<parent_component>(e);
                OBLO_ASSERT(!entityParent.parent);

                entityParent.parent = parent;
            }

            {
                auto& parentChildren = registry.add<children_component>(parent);
                parentChildren.children.emplace_back(e);
            }
        }
    }

    void register_reflected_component_and_tag_types(const reflection::reflection_registry& reflection,
        ecs::type_registry* typeRegistry,
        property_registry* propertyRegistry)
    {
        dynamic_array<reflection::type_handle> types;
        types.reserve(128);

        register_reflected_component_types(reflection, typeRegistry, propertyRegistry, types);
        register_reflected_tag_types(reflection, typeRegistry, types);
    }

    ecs::entity create_named_physical_entity(ecs::entity_registry& registry,
        const ecs::component_and_tag_sets& extraComponentsOrTags,
        string_view name,
        ecs::entity parent,
        const vec3& position,
        const quaternion& rotation,
        const vec3& scale)
    {
        const auto& types = registry.get_type_registry();

        auto builtIn = ecs::make_type_sets<name_component,
            position_component,
            rotation_component,
            scale_component,
            global_transform_component>(types);

        if (parent)
        {
            builtIn.components.add(types.find_component<parent_component>());
        }

        auto typeSets = extraComponentsOrTags;
        typeSets.components.add(builtIn.components);

        const auto e = registry.create(typeSets);

        registry.get<name_component>(e).value = name;
        registry.get<position_component>(e).value = position;
        registry.get<rotation_component>(e).value = rotation;
        registry.get<scale_component>(e).value = scale;

        auto& transform = registry.get<global_transform_component>(e);
        transform.localToWorld = make_transform_matrix(position, rotation, scale);
        transform.lastFrameLocalToWorld = transform.localToWorld;

        if (parent)
        {
            attach_root_entity_to_parent(registry, e, parent);
        }

        return e;
    }

    void reparent_entity(ecs::entity_registry& registry, ecs::entity e, ecs::entity parent)
    {
        if (!parent)
        {
            auto* const entityParent = registry.try_get<parent_component>(e);

            if (entityParent && entityParent->parent)
            {
                // Already had a parent, detach it
                auto& parentChildren = registry.get<children_component>(entityParent->parent);

                parentChildren.children.erase(
                    std::remove(parentChildren.children.begin(), parentChildren.children.end(), e));
            }

            registry.remove<parent_component>(e);
        }
        else
        {
            auto& entityParent = registry.add<parent_component>(e);

            if (entityParent.parent)
            {
                // Already had a parent, detach it
                auto& parentChildren = registry.get<children_component>(parent);

                parentChildren.children.erase(
                    std::remove(parentChildren.children.begin(), parentChildren.children.end(), e));
            }

            entityParent.parent = {};
            attach_root_entity_to_parent(registry, e, parent);
        }
    }

    ecs::entity find_parent(const ecs::entity_registry& registry, ecs::entity e)
    {
        auto* const pc = registry.try_get<parent_component>(e);
        return pc ? pc->parent : ecs::entity{};
    }

    void find_children(const ecs::entity_registry& registry, ecs::entity e, deque<ecs::entity>& outChildren)
    {
        auto* const cc = registry.try_get<children_component>(e);

        if (cc)
        {
            outChildren.append(cc->children.begin(), cc->children.end());
        }
    }

    SCENE_API ecs::entity find_root(const ecs::entity_registry& registry, ecs::entity e)
    {
        auto current = e;

        for (auto* pc = registry.try_get<parent_component>(current); pc;)
        {
            current = pc->parent;
            pc = registry.try_get<parent_component>(current);
        }

        return current;
    }

    void find_roots(const ecs::entity_registry& registry, deque<ecs::entity>& outRoots)
    {
        const auto rootsRange = registry.range<>().exclude<parent_component>();

        for (const auto& chunk : rootsRange)
        {
            const auto entities = chunk.get<ecs::entity>();
            outRoots.append(entities.begin(), entities.end());
        }
    }

    namespace
    {
        void destroy_recursive(ecs::entity_registry& registry, ecs::entity e)
        {
            auto* const cc = registry.try_get<children_component>(e);

            if (cc)
            {
                for (auto child : cc->children)
                {
                    destroy_recursive(registry, child);
                }
            }

            registry.destroy(e);
        }
    }

    void destroy_hierarchy(ecs::entity_registry& registry, ecs::entity e)
    {
        reparent_entity(registry, e, {});
        destroy_recursive(registry, e);
    }
}