#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    enum class property_kind : u8;

    struct property_node
    {
        type_id type;
        cstring_view name;
        u32 offset;
        u32 parent;
        u32 firstChild;
        u32 firstSibling;
        u32 firstProperty;
        u32 lastProperty;
        u32 firstAttribute;
        u32 lastAttribute;
    };

    struct property
    {
        type_id type;
        cstring_view name;
        property_kind kind;
        bool isEnum;
        u32 offset;
        u32 parent;
        u32 firstAttribute;
        u32 lastAttribute;
    };

    struct property_attribute
    {
        type_id type;
        const void* ptr;
    };

    struct property_tree
    {
        dynamic_array<property_node> nodes;
        dynamic_array<property> properties;
        dynamic_array<property_attribute> attributes;
    };
}