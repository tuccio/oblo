#pragma once

#include <cmath>

namespace oblo
{
    struct linear_color_tag
    {
    };

    struct srgb_color_tag
    {
    };

    inline f32 color_convert_linear_f32(srgb_color_tag, u8 value)
    {
        return std::pow(value / f32(0xffu), 2.2f);
    }

    inline f32 color_convert_linear_f32(linear_color_tag, u8 value)
    {
        return value / f32(0xffu);
    }

    inline f32 color_convert_linear_f32(linear_color_tag, f32 value)
    {
        return value;
    }

    template <typename T>
    T color_convert(linear_color_tag, srgb_color_tag, f32 source);

    template <>
    inline u8 color_convert<u8>(linear_color_tag, srgb_color_tag, f32 source)
    {
        return u8(std::pow(source, 1.f / 2.2f) * f32(0xffu));
    }

    template <typename T>
    T color_convert(linear_color_tag, linear_color_tag, f32 source);

    template <>
    inline u8 color_convert<u8>(linear_color_tag, linear_color_tag, f32 source)
    {
        return u8(source * f32(0xffu));
    }

    template <>
    inline f32 color_convert<f32>(linear_color_tag, linear_color_tag, f32 source)
    {
        return source;
    }
}