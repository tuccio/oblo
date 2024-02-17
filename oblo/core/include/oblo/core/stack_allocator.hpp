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

    template <usize Size, usize Alignment = alignof(std::max_align_t), bool AssertOnOverflow = true>
    class stack_only_allocator final : public allocator
    {
    public:
        stack_only_allocator() = default;
        stack_only_allocator(const stack_only_allocator&) = delete;
        stack_only_allocator(stack_only_allocator&&) = delete;
        stack_only_allocator& operator=(const stack_only_allocator&) = delete;
        stack_only_allocator& operator=(stack_only_allocator&&) = delete;

        byte* allocate(usize count, usize alignment) noexcept override
        {
            OBLO_ASSERT(is_power_of_two(alignment));

            auto* const ptr = reinterpret_cast<byte*>(align_power_of_two(uintptr(m_next), alignment));
            auto* const newNext = ptr + count;

            if (newNext > m_buffer + Size)
            {
                if constexpr (AssertOnOverflow)
                {
                    OBLO_ASSERT(false, "Overflow on stack only allocator");
                }

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

        bool contains(const byte* const ptr) const noexcept
        {
            return ptr >= m_buffer && ptr < m_buffer + Size;
        }

    private:
        alignas(Alignment) byte m_buffer[Size];
        byte* m_next{m_buffer};
    };

    template <usize Size, usize Alignment = alignof(std::max_align_t)>
    class stack_fallback_allocator final : public allocator
    {
    public:
        explicit stack_fallback_allocator(allocator* fallback) : m_fallback(fallback){};

        stack_fallback_allocator() : m_fallback{select_global_allocator<Alignment>()} {}

        stack_fallback_allocator(const stack_fallback_allocator&) = delete;
        stack_fallback_allocator(stack_fallback_allocator&&) = delete;
        stack_fallback_allocator& operator=(const stack_fallback_allocator&) = delete;
        stack_fallback_allocator& operator=(stack_fallback_allocator&&) = delete;

        byte* allocate(usize count, usize alignment) noexcept override
        {
            auto* const stack = m_stack.allocate(count, alignment);

            if (stack != nullptr)
            {
                return stack;
            }

            return m_fallback->allocate(count, alignment);
        }

        void deallocate(byte* ptr, usize size, usize alignment) noexcept override
        {
            if (!m_stack.contains(ptr))
            {
                m_fallback->deallocate(ptr, size, alignment);
            }
        }

    private:
        stack_only_allocator<Size, Alignment, false> m_stack;
        allocator* m_fallback{};
    };

    template <typename T, usize N>
    using array_stack_allocator = stack_allocator<sizeof(T) * N, alignof(T)>;

    template <typename T, usize N>
    using array_stack_only_allocator = stack_fallback_allocator<sizeof(T) * N, alignof(T)>;

    template <typename T, usize N>
    using array_stack_fallback_allocator = stack_fallback_allocator<sizeof(T) * N, alignof(T)>;
}