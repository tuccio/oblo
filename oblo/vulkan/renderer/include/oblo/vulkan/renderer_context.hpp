#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class renderer;

    struct renderer_context
    {
        renderer& renderer;
        frame_allocator& frameAllocator;
    };
}