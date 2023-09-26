#pragma once

#include <oblo/core/types.hpp>

namespace oblo::reflection
{
    template <typename T>
    T& access_field(void* ptr, u32 offset)
    {
        auto* const bPtr = reinterpret_cast<std::byte*>(ptr) + offset;
        return *reinterpret_cast<T*>(bPtr);
    }

    template <typename T>
    const T& access_field(const void* ptr, u32 offset)
    {
        auto* const bPtr = reinterpret_cast<std::byte*>(ptr) + offset;
        return *reinterpret_cast<T*>(bPtr);
    }
}