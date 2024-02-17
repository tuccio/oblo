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
}