#pragma once

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

    template <typename T, std::size_t N>
    using array_stack_allocator = stack_allocator<sizeof(T) * N, alignof(T)>;
}