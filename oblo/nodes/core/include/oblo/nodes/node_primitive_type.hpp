#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    enum class node_primitive_kind : u8
    {
        none,
        boolean,
        f32,
    };

    struct node_primitive_type
    {
        uuid id{};
        string name;
        node_primitive_kind kind{node_primitive_kind::none};
    };
}