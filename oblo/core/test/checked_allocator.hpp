#pragma once

#include <oblo/core/allocator.hpp>

#include <unordered_map>

namespace oblo
{
    struct checked_allocator final : allocator
    {
        byte* allocate(usize size, usize alignment) noexcept override
        {
            auto* ptr = upstream->allocate(size, alignment);
            allocations.emplace(ptr, allocation_info{size, alignment});

            return ptr;
        }

        void deallocate(byte* ptr, usize size, usize alignment) noexcept override
        {
            const auto it = allocations.find(ptr);
            ASSERT_NE(it, allocations.end());

            const auto& info = it->second;
            ASSERT_EQ(info.size, size);
            ASSERT_EQ(info.alignment, alignment);

            allocations.erase(it);

            upstream->deallocate(ptr, size, alignment);
        }

        allocator* upstream{get_global_allocator()};

        struct allocation_info
        {
            usize size;
            usize alignment;
        };

        std::unordered_map<void*, allocation_info> allocations;
    };
}