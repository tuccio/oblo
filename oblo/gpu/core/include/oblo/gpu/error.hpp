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
        out_of_memory,
        already_initialized,
        invalid_handle,
        invalid_usage,
        not_enough_command_buffers,
    };

    template <typename T = success_tag>
    using result = expected<T, error>;
}