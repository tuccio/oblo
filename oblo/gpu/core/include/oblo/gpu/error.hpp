#pragma once

#include <oblo/core/expected.hpp>

namespace oblo::gpu
{
    enum class error
    {
        undefined,
        /// @brief A fence or queue are not ready yet.
        not_ready,
        device_lost,
        out_of_date,
        already_initialized,
        invalid_usage,
        not_enough_command_buffers,
    };

    template <typename T = success_tag>
    using result = expected<T, error>;
}