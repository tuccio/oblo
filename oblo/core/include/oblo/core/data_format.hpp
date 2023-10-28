#pragma once

#include <oblo/core/types.hpp>
#include <oblo/core/unreachable.hpp>

namespace oblo
{
    enum class data_format : u8
    {
        i8,
        i16,
        i32,
        i64,
        u8,
        u16,
        u32,
        u64,
        f32,
        f64,
        vec2,
        vec3,
        enum_max
    };

    constexpr std::pair<usize, usize> get_size_and_alignment(data_format format)
    {
        switch (format)
        {
        case data_format::i8:
            return {sizeof(i8), alignof(i8)};
        case data_format::i16:
            return {sizeof(i16), alignof(i16)};
        case data_format::i32:
            return {sizeof(i32), alignof(i32)};
        case data_format::i64:
            return {sizeof(i64), alignof(i64)};
        case data_format::u8:
            return {sizeof(u8), alignof(u8)};
        case data_format::u16:
            return {sizeof(u16), alignof(u16)};
        case data_format::u32:
            return {sizeof(u32), alignof(u32)};
        case data_format::u64:
            return {sizeof(u64), alignof(u64)};
        case data_format::f32:
            return {sizeof(f32), alignof(f32)};
        case data_format::f64:
            return {sizeof(f64), alignof(f64)};
        case data_format::vec2:
            return {sizeof(f32) * 2, alignof(f32)};
        case data_format::vec3:
            return {sizeof(f32) * 3, alignof(f32)};
        default:
            unreachable();
        }
    }
}