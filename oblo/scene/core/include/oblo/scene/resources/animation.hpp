#pragma once

#include <oblo/core/data_format.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct animation_data_ref
    {
        usize begin;
        usize end;
    };

    enum class animation_target : u8
    {
        component,
        joint,
    };

    enum class animation_interpolation : u8
    {
        linear,
        cubic,
    };

    enum class animation_data_kind : u8
    {
        any_vector,
        quaternion,
    };

    struct animation_channel
    {
        // Channels identify properties, and properties are identified by name and array indices
        // (e.g.property1.array1[index1].property2).
        animation_data_ref propertyName;
        animation_data_ref propertyArrayIndices;
        animation_data_ref data;
        animation_data_ref keyframes;

        /// @brief The joint name, only valid if the target is animation_target::joint
        animation_data_ref jointName;
        data_format format;
        animation_data_kind dataKind;
        animation_target target;
        animation_interpolation interpolation;

        /// @brief The component uuid, only valid if the target is animation_target::component
        uuid componentUuid;
    };

    using animation_time_t = f32;

    struct animation
    {
        dynamic_array<animation_channel> channels;

        // The arrays contain elements of the same alignment, thus keeping alignment intact, and can be easily
        // byte-swapped E.g. vec3 data will end up in align4, together with other similarly aligned elements, such as
        // f32, u32, etc
        dynamic_array<byte> aligned1;
        dynamic_array<byte> aligned4;

        animation_time_t timeStart;
        animation_time_t timeEnd;

        platform::endian endianness = platform::endian::native;
    } OBLO_RESOURCE();

}