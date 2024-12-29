#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/power_of_two.hpp>

#include <type_traits>

namespace oblo
{
    template <typename T>
    struct compressed_pointer_with_flags
    {
        static constexpr usize max_flags = log2_round_down_power_of_two(alignof(T));

        uintptr buffer;

        bool get_flag(u32 index) const noexcept
        {
            OBLO_ASSERT(index < max_flags);
            return (buffer & (uintptr{1} << index)) != 0;
        }

        void assign_flag(u32 index, bool v) noexcept
        {
            OBLO_ASSERT(index < max_flags);
            buffer = (buffer & ~(uintptr{1} << index)) | (uintptr{v} << uintptr(index));
        }

        void set_flag(u32 index) const noexcept
        {
            OBLO_ASSERT(index < max_flags);
            buffer |= uintptr{1} << index;
        }

        void unset_flag(u32 index) const noexcept
        {
            OBLO_ASSERT(index < max_flags);
            buffer &= ~(uintptr{1} << index);
        }

        T* get_pointer() const noexcept
        {
            constexpr uintptr mask = ~((uintptr{1} << max_flags) - 1);
            return reinterpret_cast<T*>(buffer & mask);
        }

        void set_pointer(T* v) noexcept
        {
            constexpr uintptr mask = ((uintptr{1} << max_flags) - 1);
            buffer = reinterpret_cast<uintptr>(v) | (mask & buffer);
        }

        explicit operator bool() const noexcept
        {
            return get_pointer() != nullptr;
        }

        T* operator->() const noexcept
        {
            T* const ptr = get_pointer();
            OBLO_ASSERT(ptr);
            return ptr;
        }

        T& operator*() const noexcept
        {
            T* const ptr = get_pointer();
            OBLO_ASSERT(ptr);
            return *ptr;
        }
    };
}