#pragma once

#include <oblo/asset/asset_traits.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>

#include <type_traits>

namespace oblo
{
    class any_asset
    {
    public:
        any_asset() = default;
        any_asset(const any_asset&) = delete;
        any_asset(any_asset&& other) noexcept = default;

        template <typename T>
        explicit any_asset(T&& value)
        {
            using W = wrapper<std::decay_t<T>>;
            m_wrapper = allocate_unique<W>(std::forward<T>(value));
        }

        any_asset& operator=(const any_asset&) = delete;

        any_asset& operator=(any_asset&& other) noexcept = default;

        ~any_asset() = default;

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

        type_id get_type_id() const noexcept
        {
            return m_wrapper->get_type_id();
        }

        uuid get_type_uuid() const noexcept
        {
            return m_wrapper->get_type_uuid();
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
            virtual uuid get_type_uuid() const noexcept = 0;
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

            uuid get_type_uuid() const noexcept
            {
                return asset_type<T>;
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
}