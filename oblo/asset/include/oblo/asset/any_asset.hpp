#pragma once

#include <oblo/core/type_id.hpp>

#include <type_traits>

namespace oblo
{
    class any_asset
    {
    public:
        any_asset() = default;
        any_asset(const any_asset&) = delete;
        any_asset(any_asset&& other) noexcept
        {
            m_wrapper = other.m_wrapper;
            other.m_wrapper = nullptr;
        }

        template <typename T>
        explicit any_asset(T&& value)
        {
            m_wrapper = new wrapper<std::decay_t<T>>{std::forward<T>(value)};
        }

        any_asset& operator=(const any_asset&) = delete;

        any_asset& operator=(any_asset&& other) noexcept
        {
            delete m_wrapper;
            m_wrapper = other.m_wrapper;
            other.m_wrapper = nullptr;
            return *this;
        }

        ~any_asset()
        {
            delete m_wrapper;
        }

        template <typename T, typename... Args>
        void emplace(Args&&... args)
        {
            delete m_wrapper;
            m_wrapper = new wrapper<T>{std::forward<T>(args)...};
        }

        void* try_get()
        {
            return m_wrapper ? m_wrapper->get() : nullptr;
        }

        const void* try_get() const
        {
            return m_wrapper ? m_wrapper->get() : nullptr;
        }

        template <typename T>
        T* try_get()
        {
            if (m_wrapper->get_type() == get_type_id<T>())
            {
                return static_cast<T*>(m_wrapper.get());
            }
        }

        template <typename T>
        const T* try_get() const
        {
            if (m_wrapper->get_type() == get_type_id<T>())
            {
                return static_cast<const T*>(m_wrapper.get());
            }
        }

        bool empty() const
        {
            return m_wrapper == nullptr;
        }

        type_id get_type() const
        {
            return m_wrapper->get_type();
        }

    private:
        struct any_wrapper
        {
            virtual ~any_wrapper() = default;
            virtual type_id get_type() const = 0;
            virtual void* get() = 0;
        };

        template <typename T>
        struct wrapper final : any_wrapper
        {
            template <typename... Args>
            explicit wrapper(Args&&... args) : asset{std::forward<Args>(args)...}
            {
            }

            type_id get_type() const
            {
                return get_type_id<T>();
            }

            void* get()
            {
                return &asset;
            }

            T asset;
        };

    private:
        any_wrapper* m_wrapper{};
    };
}