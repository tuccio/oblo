#pragma once

#include <oblo/core/types.hpp>
#include <oblo/gpu/forward.hpp>

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

    using buffer_usage = buffer_access;
    using texture_usage = gpu::image_resource_state;
}