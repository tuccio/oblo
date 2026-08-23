#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>

namespace oblo::ui
{
    struct texture;

    enum class texture_command_kind : u8
    {
        create,
        destroy,
        update,
    };

    /// @brief Texture format, values are equivalent to VkFormat.
    enum class texture_format
    {
        r8_unorm = 9,
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