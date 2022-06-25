#pragma once

#include <memory_resource>

namespace oblo
{
    template <std::size_t Size, std::size_t Alignment>
    class stack_allocator : public std::pmr::polymorphic_allocator<std::byte>
    {
    public:
        stack_allocator() : std::pmr::polymorphic_allocator<std::byte>{&m_resource} {}
        stack_allocator(const stack_allocator&) = delete;
        stack_allocator(stack_allocator&&) = delete;
        stack_allocator& operator=(const stack_allocator&) = delete;
        stack_allocator& operator=(stack_allocator&&) = delete;

    private:
        alignas(Alignment) char m_buffer[Size];
        std::pmr::monotonic_buffer_resource m_resource{m_buffer, Size};
    };
}