#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/nodes/node_primitive_type.hpp>

namespace oblo
{
    template <node_primitive_kind Kind>
    constexpr uuid get_node_primitive_type_id();

    template <>
    inline constexpr uuid get_node_primitive_type_id<node_primitive_kind::boolean>()
    {
        return "f6f7e858-2703-442f-8897-40aecbc40c31"_uuid;
    }
    template <>
    constexpr uuid get_node_primitive_type_id<node_primitive_kind::i32>()
    {
        return "de3421c2-2979-4fb8-8627-6f0b692abc85"_uuid;
    }

    template <>
    constexpr uuid get_node_primitive_type_id<node_primitive_kind::f32>()
    {
        return "a2a92ae2-84e2-42f3-9887-c8c48b4798b6"_uuid;
    }

    template <node_primitive_kind Kind>
    constexpr cstring_view get_node_primitive_type_name();

    template <>
    inline constexpr cstring_view get_node_primitive_type_name<node_primitive_kind::boolean>()
    {
        return "bool";
    }

    template <>
    inline constexpr cstring_view get_node_primitive_type_name<node_primitive_kind::i32>()
    {
        return "i32";
    }

    template <>
    inline constexpr cstring_view get_node_primitive_type_name<node_primitive_kind::f32>()
    {
        return "f32";
    }

    template <node_primitive_kind Kind>
    node_primitive_type make_node_primitive_type()
    {
        return node_primitive_type{
            .id = get_node_primitive_type_id<Kind>(),
            .name = string{get_node_primitive_type_name<Kind>()},
            .kind = Kind,
        };
    }
}