#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    class renderer;
    class stateful_command_buffer;

    struct renderer_context
    {
        renderer& renderer;
        frame_allocator& frameAllocator;
        stateful_command_buffer* commandBuffer;
    };
}