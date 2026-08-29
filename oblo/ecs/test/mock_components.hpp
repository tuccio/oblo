#pragma once

#include <oblo/core/types.hpp>

namespace oblo::ecs
{
    struct mock_sprite_component
    {
        u32 resourceId;
    };

    struct mock_audio_source_component
    {
        u32 resourceId;
    };

    struct mock_name_component
    {
        char name;
    };

    struct mock_selected_tag
    {
    };

    struct mock_disabled_tag
    {
    };

    struct tag_a
    {
    };

    struct tag_b
    {
    };

    struct tag_c
    {
    };

    struct u8_component
    {
        u8 foo;
    };

    struct i32_component
    {
        i32 foo;
    };

    struct f64_component
    {
        f64 foo;
    };

    struct alignas(16) aligned_uvec4
    {
        u32 data[4];

        bool operator==(const aligned_uvec4&) const = default;
    };

    struct component_with_reference
    {
        entity ref;
    };
}