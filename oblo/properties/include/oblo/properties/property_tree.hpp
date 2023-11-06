#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <string>
#include <vector>

namespace oblo
{
    enum class property_kind : u8
    {
        boolean,
        u8,
        u16,
        u32,
        u64,
        i8,
        i16,
        i32,
        i64,
        f32,
        f64,
    };

    struct property_node
    {
        type_id type;
        std::string name;
        u32 parent;
        u32 firstChild;
        u32 firstSibling;
        u32 firstProperty;
        u32 lastProperty;
        u32 offset;
    };

    struct property
    {
        type_id type;
        std::string name;
        property_kind kind;
        u32 offset;
        u32 parent;
    };

    struct property_tree
    {
        std::vector<property_node> nodes;
        std::vector<property> properties;
    };
}