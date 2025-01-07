#pragma once

#include <oblo/core/pair.hpp>
#include <oblo/core/types.hpp>

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
        uuid,
        string,
        enum_max
    };

    constexpr pair<usize, usize> get_size_and_alignment(property_kind propertyKind)
    {
        switch (propertyKind)
        {
        case property_kind::boolean:
        case property_kind::i8:
        case property_kind::u8:
            return {1, 1};

        case property_kind::i16:
        case property_kind::u16:
            return {2, 2};

        case property_kind::f32:
        case property_kind::i32:
        case property_kind::u32:
            return {4, 4};

        case property_kind::f64:
        case property_kind::i64:
        case property_kind::u64:
            return {8, 8};

        case property_kind::uuid:
            return {16, 1};

        default:
            return {0, 0};
        }
    }
}