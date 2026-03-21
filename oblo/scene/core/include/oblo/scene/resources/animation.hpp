#pragma once

#include <oblo/core/data_format.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/platform/core.hpp>

namespace oblo
{
    struct animation_data_ref
    {
        usize begin;
        usize end;
    };

    enum class animation_interpolation : u8
    {
        linear,
    };

    struct animation_channel
    {
        // Channels identify properties, and properties are identified by name and array indices (e.g.
        // mycomponent::property1.array1[index1].property2).
        animation_data_ref name;
        animation_data_ref arrayIndices;
        animation_data_ref data;
        animation_data_ref keyframes;
        data_format format;
        animation_interpolation interpolation;
        u32 nameHash;
    };

    struct animation
    {
        dynamic_array<animation_channel> channels;

        // The arrays contain elements of the same alignment, thus keeping alignment intact, and can be easily
        // byte-swapped E.g. vec3 data will end up in align4, together with other similarly aligned elements, such as
        // f32, u32, etc
        dynamic_array<byte> aligned1;
        dynamic_array<byte> aligned4;

        platform::endian endianness = platform::endian::native;
    };

    using animation_time_t = f32;
}