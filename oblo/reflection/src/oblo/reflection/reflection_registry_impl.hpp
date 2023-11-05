#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/reflection/concepts/ranged_type_erasure.hpp>
#include <oblo/reflection/handles.hpp>
#include <oblo/reflection/reflection_data.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>

#include <unordered_map>
#include <variant>
#include <vector>

namespace oblo::reflection
{
    enum class type_kind
    {
        undefined_kind,
        class_kind,
    };

    struct type_ref
    {
        type_handle typeHandle;
        u32 typeIndex;
        u32 concreteIndex;
    };

    struct type_data
    {
        type_id type;
        type_kind kind;
        u32 concreteIndex;
    };

    struct class_data
    {
        type_id type;
        std::vector<field_data> fields;
    };

    struct reflection_registry_impl
    {
        ecs::type_registry typesRegistry;
        ecs::entity_registry registry{&typesRegistry};

        std::unordered_map<type_id, ecs::entity> typesMap;
    };
}