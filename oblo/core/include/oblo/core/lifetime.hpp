#pragma once

#include <cstddef>
#include <type_traits>

namespace oblo
{
    template <typename T>
    T* start_lifetime_as(void* ptr) noexcept
    {
        auto* const bytes = new (ptr) std::byte[sizeof(T)];
        auto* const object = reinterpret_cast<T*>(bytes);
        // Make sure to dereference to start the lifetime
        (void) *object;
        return object;
    }

    template <typename T>
    const T* start_lifetime_as(const void* ptr) noexcept
    {
        return start_lifetime_as<T>(const_cast<void*>(ptr));
    }

    template <typename T>
    T* start_lifetime_as_array(void* ptr, std::size_t n) noexcept
    {
        auto* const bytes = new (ptr) std::byte[sizeof(T) * n];
        auto* const object = reinterpret_cast<T*>(bytes);
        // Make sure to dereference to start the lifetime
        (void) *object;
        return object;
    }

    template <typename T>
    const T* start_lifetime_as_array(const void* ptr, std::size_t n) noexcept
    {
        return start_lifetime_as_array<T>(const_cast<void*>(ptr), n);
    }
}