#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/math/power_of_two.hpp>

#include <memory_resource>

namespace oblo
{
    template <std::size_t Size, std::size_t Alignment = alignof(std::max_align_t)>
    class stack_allocator final : public std::pmr::polymorphic_allocator<std::byte>
    {
    public:
        stack_allocator() : std::pmr::polymorphic_allocator<std::byte>{&m_resource} {}
        stack_allocator(const stack_allocator&) = delete;
        stack_allocator(stack_allocator&&) = delete;
        stack_allocator& operator=(const stack_allocator&) = delete;
        stack_allocator& operator=(stack_allocator&&) = delete;

        using std::pmr::polymorphic_allocator<std::byte>::allocate;

        std::byte* allocate(std::size_t count, std::size_t alignment)
        {
            return static_cast<std::byte*>(m_resource.allocate(count, alignment));
        }

        void deallocate(std::byte* const, const std::size_t) {}

    private:
        alignas(Alignment) char m_buffer[Size];
        std::pmr::monotonic_buffer_resource m_resource{m_buffer, Size};
    };

    template <usize Size, usize Alignment = alignof(std::max_align_t)>
    class stack_allocator_v2 : public allocator
    {
    public:
        stack_allocator_v2() = default;
        stack_allocator_v2(const stack_allocator_v2&) = delete;
        stack_allocator_v2(stack_allocator_v2&&) = delete;
        stack_allocator_v2& operator=(const stack_allocator_v2&) = delete;
        stack_allocator_v2& operator=(stack_allocator_v2&&) = delete;

        byte* allocate(usize count, usize alignment) noexcept override
        {
            OBLO_ASSERT(is_power_of_two(alignment));

            auto* const ptr = reinterpret_cast<byte*>(align_power_of_two(uintptr(m_next), alignment));
            auto* const newNext = ptr + count;

            if (newNext > m_buffer + Size)
            {
                return nullptr;
            }

            m_next = newNext;
            return ptr;
        }

        void deallocate(byte* const, const usize, const usize) noexcept override {}

        void reset()
        {
            m_next = m_buffer;
        }

    private:
        alignas(Alignment) byte m_buffer[Size];
        byte* m_next{m_buffer};
    };

    template <typename T, usize N>
    using array_stack_allocator = stack_allocator<sizeof(T) * N, alignof(T)>;

    template <typename T, usize N>
    using array_stack_allocator_v2 = stack_allocator_v2<sizeof(T) * N, alignof(T)>;
}