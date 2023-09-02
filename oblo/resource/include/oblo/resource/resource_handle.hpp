#pragma once

#include <oblo/core/type_id.hpp>

namespace oblo
{
    struct resource;

    class resource_ptr
    {
    public:
        resource_ptr() = default;

        resource_ptr(const resource_ptr&);

        resource_ptr(resource_ptr&& other) noexcept
        {
            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            other.m_ptr = nullptr;
            other.m_resource = nullptr;
        }

        explicit resource_ptr(resource* resource);

        ~resource_ptr()
        {
            reset();
        }

        resource_ptr& operator=(const resource_ptr&);

        resource_ptr& operator=(resource_ptr&& other) noexcept
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