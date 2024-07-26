#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    struct resource;
    struct uuid;

    namespace detail
    {
        void resource_release(resource* resource);
        void resource_acquire(resource* resource);
        void* resource_data(resource* resource);
        type_id resource_type(resource* resource);
        string_view resource_name(resource* resource);
        uuid resource_uuid(resource* resource);
    }

    template <typename T = void>
    class resource_ptr
    {
    public:
        resource_ptr() = default;

        resource_ptr(const resource_ptr& other)
        {
            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            if (m_resource)
            {
                detail::resource_acquire(m_resource);
            }
        }

        resource_ptr(resource_ptr&& other) noexcept
        {
            m_ptr = other.m_ptr;
            m_resource = other.m_resource;

            other.m_ptr = nullptr;
            other.m_resource = nullptr;
        }

        explicit resource_ptr(resource* resource)
        {
            if (resource)
            {
                detail::resource_acquire(resource);
                m_resource = resource;
                m_ptr = detail::resource_data(resource);
            }
        }

        ~resource_ptr()
        {
            reset();
        }

        resource_ptr& operator=(const resource_ptr& other)
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

        resource_ptr& operator=(resource_ptr&& other) noexcept
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

        string_view get_name() const noexcept
        {
            return detail::resource_name(m_resource);
        }

        uuid get_id() const noexcept
        {
            return detail::resource_uuid(m_resource);
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
        resource_ptr<U> as() && noexcept
        {
            resource_ptr<U> other;

            if (m_resource && get_type_id<U>() == get_type())
            {
                other.m_ptr = static_cast<const U*>(m_ptr);
                other.m_resource = m_resource;
                m_ptr = nullptr;
                m_resource = nullptr;
            }

            return other;
        }

        template <typename U>
        resource_ptr<U> as() const& noexcept
        {
            resource_ptr<U> other;

            if (m_resource && get_type_id<U>() == get_type())
            {
                other.m_ptr = static_cast<const U*>(m_ptr);
                other.m_resource = m_resource;
                detail::resource_acquire(m_resource);
            }

            return other;
        }

        resource_ref<T> as_ref() const noexcept
        {
            if (m_resource)
            {
                return resource_ref<T>{.id = get_id()};
            }

            return {};
        }

    private:
        template <typename>
        friend class resource_ptr;

    private:
        const T* m_ptr{};
        resource* m_resource{};
    };
}