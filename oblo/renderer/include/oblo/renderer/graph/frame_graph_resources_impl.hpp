#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/gpu/structs.hpp>

namespace oblo
{
    struct frame_graph_texture_impl
    {
        h32<gpu::image> handle;
        gpu::image_descriptor descriptor;
    };

    struct frame_graph_buffer_impl
    {
        h32<gpu::buffer> handle;
        u64 offset;
        u64 size;
    };
}