#pragma once

#include <oblo/core/allocator.hpp>

#include <memory_resource>

namespace oblo
{
    class memory_resource_adapter final : public std::pmr::memory_resource
    {
    public:
        explicit memory_resource_adapter(allocator* allocator) : m_allocator{allocator} {}
        memory_resource_adapter(const memory_resource_adapter&) = default;
        memory_resource_adapter& operator=(const memory_resource_adapter&) = default;
        memory_resource_adapter() = default;

        allocator* get_allocator() const noexcept
        {
            return m_allocator;
        }

    private:
        void* do_allocate(usize size, usize alignment) override
        {
            return m_allocator->allocate(size, alignment);
        }

        void do_deallocate(void* ptr, usize size, usize alignment) override
        {
            return m_allocator->deallocate(static_cast<byte*>(ptr), size, alignment);
        }

        bool do_is_equal(const memory_resource&) const noexcept override
        {
            return false;
        }

        allocator* m_allocator{};
    };
}