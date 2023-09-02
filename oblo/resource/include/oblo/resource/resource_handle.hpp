#pragma once

#include <oblo/core/type_id.hpp>

namespace oblo
{
    struct resource;

    class resource_handle
    {
    public:
        resource_handle() = default;

        resource_handle(const resource_handle&);

        resource_handle(resource_handle&& other) noexcept
        {
            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            other.m_ptr = nullptr;
            other.m_resource = nullptr;
        }

        explicit resource_handle(resource* resource);

        ~resource_handle()
        {
            reset();
        }

        resource_handle& operator=(const resource_handle&);

        resource_handle& operator=(resource_handle&& other) noexcept
        {
            reset();
            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            other.m_ptr = nullptr;
            other.m_resource = nullptr;
            return *this;
        }

        void reset();

        type_id get_type() const noexcept;

        template <typename T>
        const T* get() const noexcept
        {
            return static_cast<const T*>(m_ptr);
        }

        template <typename T>
        bool is() const noexcept
        {
            return get_type() == get_type_id<T>();
        }

        explicit operator bool() const noexcept
        {
            return m_ptr != nullptr;
        }

    private:
        const void* m_ptr{};
        resource* m_resource{};
    };
}