#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    class allocator
    {
    public:
        virtual byte* allocate(usize size, usize alignment) noexcept = 0;
        virtual void deallocate(byte* ptr, usize size, usize alignment) noexcept = 0;
    };

    allocator* get_global_allocator() noexcept;
    allocator* get_global_aligned_allocator() noexcept;

    template <usize Alignment>
    allocator* select_global_allocator()
    {
        if constexpr (Alignment <= alignof(std::max_align_t))
        {
            return get_global_allocator();
        }
        else
        {
            return get_global_aligned_allocator();
        }
    }
}