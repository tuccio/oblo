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
        void resource_start_loading(resource* resource);
        bool resource_is_loaded(resource* resource);
        void resource_load_sync(resource* resource);
        bool resource_is_invalidated(resource* resource);
        void resource_invalidate(resource* resource);
    }

    template <typename T = void>
    class resource_ptr
    {
    public:
        resource_ptr() = default;

        resource_ptr(const resource_ptr& other)
        {
            m_resource = other.m_resource;

            if (m_resource)
            {
                detail::resource_acquire(m_resource);
            }
        }

        resource_ptr(resource_ptr&& other) noexcept
        {
            m_resource = other.m_resource;
            other.m_resource = nullptr;
        }

        explicit resource_ptr(resource* resource)
        {
            if (resource)
            {
                detail::resource_acquire(resource);
                m_resource = resource;
            }
        }

        ~resource_ptr()
        {
            reset();
        }

        resource_ptr& operator=(const resource_ptr& other)
        {
            reset();

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
            m_resource = other.m_resource;

            other.m_resource = nullptr;
            return *this;
        }

        void reset()
        {
            if (m_resource)
            {
                detail::resource_release(m_resource);
            }

            m_resource = nullptr;
        }

        type_id get_type_id() const noexcept
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
            return static_cast<const T*>(detail::resource_data(m_resource));
        }

        const T* operator->() const noexcept
        {
            const T* const ptr = get();
            OBLO_ASSERT(ptr, "The resource is not loaded");
            return ptr;
        }

        decltype(auto) operator*() const noexcept
            requires(!std::is_void_v<T>)
        {
            const T* const ptr = get();
            OBLO_ASSERT(ptr, "The resource is not loaded");
            return *ptr;
        }

        explicit operator bool() const noexcept
        {
            return m_resource != nullptr;
        }

        template <typename U>
        resource_ptr<U> as() && noexcept
        {
            resource_ptr<U> other;

            if (m_resource && oblo::get_type_id<U>() == get_type_id())
            {
                other.m_resource = m_resource;
                m_resource = nullptr;
            }

            return other;
        }

        template <typename U>
        resource_ptr<U> as() const& noexcept
        {
            resource_ptr<U> other;

            if (m_resource && oblo::get_type_id<U>() == get_type_id())
            {
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

        void load_start_async() const noexcept
        {
            if (m_resource)
            {
                detail::resource_start_loading(m_resource);
            }
        }

        void load_sync() const noexcept
        {
            if (m_resource)
            {
                detail::resource_load_sync(m_resource);
            }
        }

        bool is_loaded() const noexcept
        {
            if (m_resource)
            {
                return detail::resource_is_loaded(m_resource);
            }

            return false;
        }

        bool is_invalidated() const noexcept
        {
            if (m_resource)
            {
                return detail::resource_is_invalidated(m_resource);
            }

            return false;
        }

        void invalidate() const noexcept
        {
            if (m_resource)
            {
                detail::resource_invalidate(m_resource);
            }
        }

    private:
        template <typename>
        friend class resource_ptr;

    private:
        resource* m_resource{};
    };
}