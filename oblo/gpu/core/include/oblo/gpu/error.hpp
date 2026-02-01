#pragma once

namespace oblo::gpu
{
    enum class error
    {
        undefined,
        device_lost,
        already_initialized,
    };
}