#pragma once

#include <oblo/core/debug.hpp>

namespace oblo
{
    // Implementation from https://en.cppreference.com/w/cpp/utility/unreachable
    [[noreturn]] inline void unreachable()
    {
        OBLO_ASSERT(false, "Code was marked as unreachable");

#if defined(__GNUC__) || defined(__clang__)
        __builtin_unreachable();
#elif defined(_MSC_VER) // MSVC
        __assume(false);
#endif
    }
}