#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    enum class node_primitive_kind : u8
    {
        boolean,
        f32,
        enum_max,
    };

    struct node_primitive_type
    {
        uuid id{};
        string name;
        node_primitive_kind kind{node_primitive_kind::enum_max};
    };
}