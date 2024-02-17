#include <oblo/core/allocator.hpp>

#include <oblo/core/debug.hpp>

#include <cstdlib>

namespace oblo
{
    namespace
    {
        struct global_allocator final : allocator
        {
            byte* allocate(usize size, [[maybe_unused]] usize alignment) noexcept override
            {
                OBLO_ASSERT(alignment <= alignof(std::max_align_t));
                return reinterpret_cast<byte*>(std::malloc(size));
            }

            void deallocate(byte* ptr, usize, usize) noexcept override
            {
                std::free(ptr);
            }
        };

        global_allocator g_allocator;
    }

    allocator* get_global_allocator() noexcept
    {
        return &g_allocator;
    }
}