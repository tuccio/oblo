#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    enum class buffer_access : u8
    {
        storage_read,
        storage_write,
        storage_upload,
        download,
        uniform,
        vertex,
        index,
        indirect,
        enum_max,
    };

    enum class texture_access : u8
    {
        render_target_write,
        depth_stencil_read,
        depth_stencil_write,
        shader_read,
        storage_read,
        storage_write,
        transfer_source,
        transfer_destination,
        present,
        enum_max,
    };
}