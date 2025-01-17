#pragma once

#include <oblo/core/allocator.hpp>

#include <type_traits>

namespace oblo
{
    template <typename T>
    struct unique_ptr_default_deleter
    {
        unique_ptr_default_deleter() : m_allocator{select_global_allocator<alignof(T)>()} {}
        explicit unique_ptr_default_deleter(allocator* a) : m_allocator{a} {}

        unique_ptr_default_deleter(const unique_ptr_default_deleter& other) = default;
        unique_ptr_default_deleter(unique_ptr_default_deleter&& other) noexcept = default;

        unique_ptr_default_deleter& operator=(const unique_ptr_default_deleter& other) = default;
        unique_ptr_default_deleter& operator=(unique_ptr_default_deleter&& other) noexcept = default;

        template <typename T>
        void operator()(T* ptr) const
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                ptr->~T();
            }

            m_allocator->deallocate(reinterpret_cast<byte*>(ptr), sizeof(T), alignof(T));
        }

        allocator* get_allocator() const noexcept
        {
            return m_allocator;
        }

        allocator* m_allocator{};
    };

    template <typename T>
    struct unique_ptr_default_deleter<T[]>
    {
        unique_ptr_default_deleter() : m_allocator{select_global_allocator<alignof(T)>()} {}
        explicit unique_ptr_default_deleter(allocator* a) : m_allocator{a} {}

        unique_ptr_default_deleter(const unique_ptr_default_deleter& other) = default;
        unique_ptr_default_deleter(unique_ptr_default_deleter&& other) noexcept = default;

        unique_ptr_default_deleter& operator=(const unique_ptr_default_deleter& other) = default;
        unique_ptr_default_deleter& operator=(unique_ptr_default_deleter&& other) noexcept = default;

        template <typename T>
        void operator()(T* ptr, usize count) const
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (usize i = 0; i < count; ++i)
                {
                    ptr[i].~T();
                }
            }

            m_allocator->deallocate(reinterpret_cast<byte*>(ptr), sizeof(T) * count, alignof(T));
        }

        allocator* get_allocator() const noexcept
        {
            return m_allocator;
        }

        allocator* m_allocator{};
    };

    template <typename T, typename D = unique_ptr_default_deleter<T>>
    class unique_ptr final : D
    {
    public:
        unique_ptr() = default;
        unique_ptr(const unique_ptr&) = delete;

        template <typename... Args>
        unique_ptr(T* ptr, usize count, Args&&... allocArgs) : D{std::forward<Args>(allocArgs)...}, m_ptr{ptr}
        {
        }

        unique_ptr(unique_ptr&& other) noexcept : m_ptr{other.m_ptr}
        {
            other.m_ptr = nullptr;
        }

        ~unique_ptr()
        {
            reset();
        }

        unique_ptr& operator=(unique_ptr&& other) noexcept
        {
            reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
            return *this;
        }

        void reset(T* ptr = nullptr) noexcept
        {
            if (m_ptr)
            {
                D::operator()(m_ptr);
            }

            m_ptr = ptr;
        }

        T* release() noexcept
        {
            T* const ptr = m_ptr;
            m_ptr = nullptr;
            return ptr;
        }

        explicit operator bool() const noexcept
        {
            return m_ptr != nullptr;
        }

        auto get_allocator() const noexcept
        {
            return D::get_allocator();
        }

    private:
        T* m_ptr{};
    };

    template <typename T, typename D>
    class unique_ptr<T[], D> final : D
    {
    public:
        unique_ptr() = default;
        unique_ptr(const unique_ptr&) = delete;

        template <typename... Args>
        unique_ptr(T* ptr, usize count, Args&&... allocArgs) :
            D{std::forward<Args>(allocArgs)...}, m_ptr{ptr}, m_count{count}
        {
        }

        unique_ptr(unique_ptr&& other) noexcept : m_ptr{other.m_ptr}, m_count{other.m_count}
        {
            other.m_ptr = nullptr;
            other.m_count = 0;
        }

        ~unique_ptr()
        {
            reset();
        }

        unique_ptr& operator=(unique_ptr&& other) noexcept
        {
            reset();
            m_ptr = other.m_ptr;
            m_count = other.m_count;
            other.m_ptr = nullptr;
            other.m_count = 0;
            return *this;
        }

        void reset(T* ptr = nullptr, usize count = 0) noexcept
        {
            if (m_ptr)
            {
                D::operator()(m_ptr, count);
            }

            m_ptr = ptr;
            m_count = count;
        }

        T* release() noexcept
        {
            T* const ptr = m_ptr;
            m_ptr = nullptr;
            m_count = 0;
            return ptr;
        }

        explicit operator bool() const noexcept
        {
            return m_ptr != nullptr;
        }

        auto get_allocator() const noexcept
        {
            return D::get_allocator();
        }

        T* data() const
        {
            return m_ptr;
        }

        usize size() const
        {
            return m_count;
        }

    private:
        T* m_ptr{};
        usize m_count{};
    };
}