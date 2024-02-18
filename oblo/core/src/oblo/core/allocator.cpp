#include <oblo/core/allocator.hpp>

#include <oblo/core/debug.hpp>

#include <cstdlib>
#include <memory>

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

        struct global_aligned_allocator final : allocator
        {
            byte* allocate(usize size, usize alignment) noexcept override
            {
                return static_cast<byte*>(::operator new(size, std::align_val_t(alignment)));
            }

            void deallocate(byte* ptr, usize, usize alignment) noexcept override
            {
                ::operator delete(ptr, std::align_val_t(alignment));
            }
        };

        global_allocator g_allocator;
        global_aligned_allocator g_alignedAllocator;
    }

    allocator* get_global_allocator() noexcept
    {
        return &g_allocator;
    }

    allocator* get_global_aligned_allocator() noexcept
    {
        return &g_alignedAllocator;
    }
}