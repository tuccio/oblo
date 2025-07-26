#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <type_traits>

namespace oblo
{
    class any
    {
    public:
        any() = default;
        any(const any&) = delete;
        any(any&& other) noexcept = default;

        template <typename T>
        explicit any(T&& value)
        {
            using W = wrapper<std::decay_t<T>>;
            m_wrapper = allocate_unique<W>(std::forward<T>(value));
        }

        any& operator=(const any&) = delete;

        any& operator=(any&& other) noexcept = default;

        ~any() = default;

        template <typename T, typename... Args>
        T& emplace(Args&&... args)
        {
            using W = wrapper<std::decay_t<T>>;
            m_wrapper = allocate_unique<W>(std::forward<Args>(args)...);
            return *as<T>();
        }

        void* as() noexcept
        {
            return m_wrapper ? m_wrapper->get() : nullptr;
        }

        const void* as() const noexcept
        {
            return m_wrapper ? m_wrapper->get() : nullptr;
        }

        template <typename T>
        T* as() noexcept
        {
            if (is<T>())
            {
                return static_cast<T*>(m_wrapper->get());
            }

            return nullptr;
        }

        template <typename T>
        const T* as() const noexcept
        {
            if (is<T>())
            {
                return static_cast<const T*>(m_wrapper->get());
            }

            return nullptr;
        }

        bool empty() const noexcept
        {
            return m_wrapper == nullptr;
        }

        void clear()
        {
            m_wrapper.reset();
        }

        type_id get_type_id() const noexcept
        {
            return m_wrapper->get_type_id();
        }

        template <typename T>
        bool is() const noexcept
        {
            return !empty() && m_wrapper->get_type_id() == oblo::get_type_id<T>();
        }

        explicit operator bool() const noexcept
        {
            return bool{m_wrapper};
        }

    private:
        struct any_wrapper
        {
            virtual ~any_wrapper() = default;
            virtual type_id get_type_id() const noexcept = 0;
            virtual void* get() noexcept = 0;
        };

        template <typename T>
        struct wrapper final : any_wrapper
        {
            template <typename... Args>
            explicit wrapper(Args&&... args) : asset{std::forward<Args>(args)...}
            {
            }

            type_id get_type_id() const noexcept
            {
                return oblo::get_type_id<T>();
            }

            void* get() noexcept
            {
                return &asset;
            }

            T asset;
        };

    private:
        unique_ptr<any_wrapper> m_wrapper{};
    };

    template <typename T, typename... Args>
    any make_any(Args&&... args)
    {
        any r;
        r.emplace<T>(std::forward<Args>(args)...);
        return r;
    }
}