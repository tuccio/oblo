#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/reflection/handles.hpp>
#include <oblo/reflection/reflection_data.hpp>

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
        type_handle typeHandle;
        std::vector<field_data> fields;
    };

    struct reflection_registry_impl
    {
        std::unordered_map<type_id, type_handle> typesMap;
        std::vector<type_data> types;
        std::vector<class_data> classes;
    };
}