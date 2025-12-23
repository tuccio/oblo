#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/platform/compiler.hpp>

#include <type_traits>
#include <utility>

namespace oblo
{
    template <typename T>
    struct unique_ptr_default_deleter
    {
        unique_ptr_default_deleter() : m_allocator{select_global_allocator<alignof(T)>()} {}
        explicit unique_ptr_default_deleter(allocator* a) : m_allocator{a} {}

        unique_ptr_default_deleter(const unique_ptr_default_deleter& other) = default;
        unique_ptr_default_deleter(unique_ptr_default_deleter&& other) noexcept = default;

        template <typename U>
        unique_ptr_default_deleter(const unique_ptr_default_deleter<U>& other) : m_allocator{other.get_allocator()}
        {
        }

        unique_ptr_default_deleter& operator=(const unique_ptr_default_deleter& other) = default;
        unique_ptr_default_deleter& operator=(unique_ptr_default_deleter&& other) noexcept = default;

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

    private:
        allocator* m_allocator{};
    };

    template <typename T>
    struct unique_ptr_default_deleter<T[]>
    {
        unique_ptr_default_deleter() : m_allocator{select_global_allocator<alignof(T)>()} {}
        explicit unique_ptr_default_deleter(allocator* a) : m_allocator{a} {}

        unique_ptr_default_deleter(const unique_ptr_default_deleter& other) = default;
        unique_ptr_default_deleter(unique_ptr_default_deleter&& other) noexcept = default;

        template <typename U>
        unique_ptr_default_deleter(const unique_ptr_default_deleter<U>& other) : m_allocator{other.get_allocator()}
        {
        }

        unique_ptr_default_deleter& operator=(const unique_ptr_default_deleter& other) = default;
        unique_ptr_default_deleter& operator=(unique_ptr_default_deleter&& other) noexcept = default;

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
        using deleter_type = D;

    public:
        unique_ptr() = default;
        unique_ptr(const unique_ptr&) = delete;

        template <typename... Args>
        explicit unique_ptr(T* ptr, Args&&... allocArgs) : D{std::forward<Args>(allocArgs)...}, m_ptr{ptr}
        {
        }

        unique_ptr(unique_ptr&& other) noexcept : D{std::move(other.get_deleter())}, m_ptr{other.m_ptr}
        {
            other.m_ptr = nullptr;
        }

        template <typename U, typename E>
        unique_ptr(unique_ptr<U, E>&& other) noexcept : D{std::move(other.get_deleter())}, m_ptr{other.m_ptr}
        {
            other.m_ptr = nullptr;
        }

        unique_ptr(std::nullptr_t) : unique_ptr{} {}

        ~unique_ptr()
        {
            reset();
        }

        unique_ptr& operator=(unique_ptr&& other) noexcept
        {
            reset();
            m_ptr = other.m_ptr;
            get_deleter() = std::move(other.get_deleter());
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

        OBLO_FORCEINLINE explicit operator bool() const noexcept
        {
            return m_ptr != nullptr;
        }

        D& get_deleter() noexcept
        {
            return static_cast<D&>(*this);
        }

        const D& get_deleter() const noexcept
        {
            return static_cast<D&>(*this);
        }

        OBLO_FORCEINLINE T* get() const noexcept
        {
            return m_ptr;
        }

        OBLO_FORCEINLINE T* operator->() const noexcept
        {
            OBLO_ASSERT(m_ptr);
            return m_ptr;
        }

        OBLO_FORCEINLINE T& operator*() const noexcept
        {
            OBLO_ASSERT(m_ptr);
            return *m_ptr;
        }

        bool operator==(const unique_ptr& other) const noexcept
        {
            return m_ptr == other.m_ptr;
        }

        bool operator!=(const unique_ptr& other) const noexcept
        {
            return m_ptr != other.m_ptr;
        }

        bool operator==(const T* other) const noexcept
        {
            return m_ptr == other;
        }

        bool operator!=(const T* other) const noexcept
        {
            return m_ptr != other;
        }

    private:
        template <typename U, typename E>
        friend class unique_ptr;

    private:
        T* m_ptr{};
    };

    template <typename T, typename D>
    class unique_ptr<T[], D> final : D
    {
    public:
        using deleter_type = D;

    public:
        unique_ptr() = default;
        unique_ptr(const unique_ptr&) = delete;

        template <typename... Args>
        unique_ptr(T* ptr, usize count, Args&&... allocArgs) :
            D{std::forward<Args>(allocArgs)...}, m_ptr{ptr}, m_size{count}
        {
        }

        unique_ptr(unique_ptr&& other) noexcept :
            D{std::move(other.get_deleter())}, m_ptr{other.m_ptr}, m_size{other.m_size}
        {
            other.m_ptr = nullptr;
            other.m_size = 0;
        }

        ~unique_ptr()
        {
            reset();
        }

        unique_ptr& operator=(unique_ptr&& other) noexcept
        {
            reset();
            m_ptr = other.m_ptr;
            m_size = other.m_size;
            get_deleter() = std::move(other.get_deleter());
            other.m_ptr = nullptr;
            other.m_size = 0;
            return *this;
        }

        void reset(T* ptr, usize count) noexcept
        {
            if (m_ptr)
            {
                D::operator()(m_ptr, m_size);
            }

            m_ptr = ptr;
            m_size = count;
        }

        void reset() noexcept
        {
            reset(nullptr, 0);
        }

        T* release() noexcept
        {
            T* const ptr = m_ptr;
            m_ptr = nullptr;
            m_size = 0;
            return ptr;
        }

        explicit operator bool() const noexcept
        {
            return m_ptr != nullptr;
        }

        D& get_deleter() noexcept
        {
            return static_cast<D&>(*this);
        }

        const D& get_deleter() const noexcept
        {
            return static_cast<D&>(*this);
        }

        T* get() const noexcept
        {
            return m_ptr;
        }

        T* data() const noexcept
        {
            return m_ptr;
        }

        usize size() const noexcept
        {
            return m_size;
        }

        T& operator[](usize index) const noexcept
        {
            OBLO_ASSERT(index < m_size);
            return m_ptr[index];
        }

        bool operator==(const unique_ptr& other) const noexcept
        {
            return m_ptr == other.m_ptr;
        }

        bool operator!=(const unique_ptr& other) const noexcept
        {
            return m_ptr != other.m_ptr;
        }

        bool operator==(const T* other) const noexcept
        {
            return m_ptr == other;
        }

        bool operator!=(const T* other) const noexcept
        {
            return m_ptr != other;
        }

    private:
        T* m_ptr{};
        usize m_size{};
    };

    template <typename T, typename... Args>
    unique_ptr<T> allocate_unique(Args&&... args)
        requires(!std::is_array_v<T>)
    {
        unique_ptr<T> r;
        auto* const allocator = r.get_deleter().get_allocator();
        T* const ptr = new (allocator->allocate(sizeof(T), alignof(T))) T{std::forward<Args>(args)...};
        r.reset(ptr);
        return r;
    }

    template <typename T, typename... Args>
    unique_ptr<T> allocate_unique_default()
        requires(!std::is_array_v<T>)
    {
        unique_ptr<T> r;
        auto* const allocator = r.get_deleter().get_allocator();
        T* const ptr = new (allocator->allocate(sizeof(T), alignof(T))) T;
        r.reset(ptr);
        return r;
    }

    template <typename T>
    unique_ptr<T> allocate_unique(usize count)
        requires(std::is_array_v<T>)
    {
        using U = std::remove_all_extents_t<T>;
        unique_ptr<T> r;
        auto* const allocator = r.get_deleter().get_allocator();
        U* const ptr = new (allocator->allocate(sizeof(U) * count, alignof(U))) U[count]{};
        r.reset(ptr, count);
        return r;
    }
}