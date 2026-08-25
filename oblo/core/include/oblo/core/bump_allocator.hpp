#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/power_of_two.hpp>

namespace oblo
{
    class bump_allocator final : public allocator
    {
    public:
        bump_allocator() = default;

        bump_allocator(usize chunkSize, allocator* upstream = get_global_allocator()) :
            m_upstream{upstream}, m_chunkSize{chunkSize}
        {
        }

        bump_allocator(const bump_allocator&) = delete;
        bump_allocator(bump_allocator&&) noexcept = delete;

        bump_allocator& operator=(const bump_allocator&) = delete;
        bump_allocator& operator=(bump_allocator&&) noexcept = delete;

        ~bump_allocator()
        {
            destroy_all();
        }

        byte* allocate(usize size, usize alignment) noexcept final
        {
            OBLO_ASSERT(alignment != 0 && is_power_of_two(alignment));
            chunk* current = m_current;

            // Try the current chunk first.
            if (current)
            {
                if (byte* result = try_allocate(current, size, alignment))
                {
                    return result;
                }

                // Try existing subsequent chunks.
                while (current->next)
                {
                    current = current->next;

                    if (byte* result = try_allocate(current, size, alignment))
                    {
                        m_current = current;
                        return result;
                    }
                }
            }

            current = allocate_chunk(size, alignment);

            if (!current)
            {
                return nullptr;
            }

            if (m_current)
            {
                m_current->next = current;
            }
            else
            {
                m_first = current;
            }

            m_current = current;

            return try_allocate(current, size, alignment);
        }

        void deallocate(byte*, usize, usize) noexcept final {}

        void reset() noexcept
        {
            for (chunk* current = m_first; current; current = current->next)
            {
                current->current = chunk_begin(current);
            }

            m_current = m_first;
        }

        void destroy_all() noexcept
        {
            chunk* current = m_first;

            while (current)
            {
                chunk* const next = current->next;
                const usize chunkSize = current->end - reinterpret_cast<byte*>(current);
                m_upstream->deallocate(reinterpret_cast<byte*>(current), chunkSize, alignof(chunk));
                current = next;
            }

            m_first = nullptr;
            m_current = nullptr;
        }

    private:
        struct chunk
        {
            chunk* next;
            byte* current;
            byte* end;
            usize size;
        };

        static byte* chunk_begin(chunk* c) noexcept
        {
            return reinterpret_cast<byte*>(c) + sizeof(chunk);
        }

        static byte* align_ptr(byte* ptr, usize alignment) noexcept
        {
            const uintptr address = reinterpret_cast<uintptr>(ptr);
            const uintptr mask = static_cast<uintptr>(alignment - 1);

            const uintptr aligned = address + ((-address) & mask);

            return reinterpret_cast<byte*>(aligned);
        }

        static byte* try_allocate(chunk* c, usize size, usize alignment) noexcept
        {
            byte* const result = align_ptr(c->current, alignment);

            if (result > c->end)
            {
                return nullptr;
            }

            const usize remaining = static_cast<usize>(c->end - result);

            if (remaining < size)
            {
                return nullptr;
            }

            c->current = result + size;
            return result;
        }

        chunk* allocate_chunk(usize size, usize alignment) noexcept
        {
            constexpr usize maxValue = ~usize{};

            // Account for the chunk header and worst-case alignment padding.
            if (size > maxValue - sizeof(chunk))
            {
                return nullptr;
            }

            const usize required = size + sizeof(chunk);

            if (alignment - 1 > maxValue - required)
            {
                return nullptr;
            }

            const usize minSize = required + alignment - 1;

            const usize allocatedSize = m_chunkSize > minSize ? m_chunkSize : minSize;

            byte* const memory = m_upstream->allocate(allocatedSize, alignof(chunk));

            if (!memory)
            {
                return nullptr;
            }

            chunk* const result = new (memory) chunk{
                .next = nullptr,
                .current = memory + sizeof(chunk),
                .end = memory + allocatedSize,
            };

            return result;
        }

    private:
        allocator* m_upstream{get_global_allocator()};

        chunk* m_first{};
        chunk* m_current{};

        usize m_chunkSize{1u << 20};
    };
}
