#pragma once

namespace oblo::gpu
{
    enum class error
    {
        undefined,
        device_lost,
        out_of_date,
        already_initialized,
    };
}