#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/reflection/reflection_data.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>

#include <unordered_map>
#include <vector>

namespace oblo::reflection
{
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