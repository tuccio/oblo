#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/type_id.hpp>

namespace oblo::resource
{
    struct resource;

    namespace detail
    {
        void resource_release(resource* resource);
        void resource_acquire(resource* resource);
        void* resource_data(resource* resource);
        type_id resource_type(resource* resource);
    }

    template <typename T = void>
    class ptr
    {
    public:
        ptr() = default;

        ptr(const ptr& other)
        {
            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            if (m_resource)
            {
                detail::resource_acquire(m_resource);
            }
        }

        ptr(ptr&& other) noexcept
        {
            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            other.m_ptr = nullptr;
            other.m_resource = nullptr;
        }

        explicit ptr(resource* resource)
        {
            if (resource)
            {
                detail::resource_acquire(resource);
                m_resource = resource;
                m_ptr = detail::resource_data(resource);
            }
        }

        ~ptr()
        {
            reset();
        }

        ptr& operator=(const ptr&)
        {
            reset();

            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            if (m_resource)
            {
                detail::resource_acquire(m_resource);
            }

            return *this;
        }

        ptr& operator=(ptr&& other) noexcept
        {
            reset();
            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            other.m_ptr = nullptr;
            other.m_resource = nullptr;
            return *this;
        }

        void reset()
        {
            if (m_resource)
            {
                detail::resource_release(m_resource);
            }

            m_ptr = nullptr;
            m_resource = nullptr;
        }

        type_id get_type() const noexcept
        {
            return detail::resource_type(m_resource);
        }

        const T* get() const noexcept
        {
            return m_ptr;
        }

        const T* operator->() const noexcept
        {
            return m_ptr;
        }

        explicit operator bool() const noexcept
        {
            return m_ptr != nullptr;
        }

        template <typename U>
        ptr<U> as() && noexcept
        {
            OBLO_ASSERT(!m_resource || get_type_id<U>() == get_type());

            ptr<U> other;
            other.m_ptr = static_cast<const U*>(m_ptr);
            other.m_resource = m_resource;
            m_ptr = nullptr;
            m_resource = nullptr;

            return other;
        }

        template <typename U>
        ptr<U> as() const& noexcept
        {
            OBLO_ASSERT(!m_resource || get_type_id<U>() == get_type());

            ptr<U> other;
            other.m_ptr = static_cast<const U*>(m_ptr);
            other.m_resource = m_resource;
            detail::resource_acquire(m_resource);

            return other;
        }

    private:
        template <typename>
        friend class ptr;

    private:
        const T* m_ptr{};
        resource* m_resource{};
    };
}