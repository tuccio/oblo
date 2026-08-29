#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>

namespace oblo::ui
{
    enum class texture_format
    {
        r8_unorm = 9,
    };

    struct texture
    {
        h32<texture> id;
        u32 width;
        u32 height;
        u32 rowPitch;
        texture_format format;
        dynamic_array<u8> data;
    };

    enum class texture_command_kind : u8
    {
        create,
        destroy,
        update,
    };

    struct texture_command_create
    {
        h32<texture> id;
        u32 width;
        u32 height;
        texture_format format;
    };

    struct texture_command_destroy
    {
        h32<texture> id;
    };

    struct texture_command_update
    {
        h32<texture> id;
        u32 x;
        u32 y;
        u32 width;
        u32 height;
    };

    struct texture_command
    {
        texture_command_kind kind;

        union {
            texture_command_create create;
            texture_command_destroy destroy;
            texture_command_update update;
        };
    };
}