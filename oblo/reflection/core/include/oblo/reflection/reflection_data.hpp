#pragma once

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

namespace oblo::reflection
{
    enum class type_kind : u8
    {
        fundamental_kind,
        class_kind,
        enum_kind,
        array_kind,
    };

    struct type_data
    {
        type_id type;
        type_kind kind;
        u32 size;
        u32 alignment;
    };

    struct attribute_data
    {
        type_id type;
        const void* ptr;
    };

    struct field_data
    {
        type_id type;
        cstring_view name;
        u32 offset;
        dynamic_array<attribute_data> attributes;
    };

    struct array_data
    {
        buffered_array<usize, 2> extents;
    };
}