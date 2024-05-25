#pragma once

namespace oblo::vk
{
    struct time_buffer
    {
        u32 frameIndex;
        u32 _padding[3];
    };
}