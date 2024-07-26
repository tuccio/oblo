#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/platform/compiler.hpp>
#include <oblo/core/stack_allocator.hpp>

namespace oblo
{
    template <typename T, usize N>
    class buffered_array
    {
    public:
        using value_type = T;

        using pointer = T*;
        using const_pointer = const T*;

        using reference = T&;
        using const_reference = const T&;

        using size_type = usize;
        using difference_type = ptrdiff;

        using iterator = T*;
        using const_iterator = const T*;

    public:
        OBLO_FORCEINLINE buffered_array()
        {
            m_array.reserve(N);
        }

        OBLO_FORCEINLINE explicit buffered_array(allocator* fallback) : m_allocator{fallback}
        {
            m_array.reserve(N);
        }

        OBLO_FORCEINLINE buffered_array(const std::initializer_list<T> initializer)
        {
            m_array.reserve(N);
            m_array = initializer;
        }

        OBLO_FORCEINLINE buffered_array(const buffered_array& other)
        {
            m_array = other.m_array;
        }

        OBLO_FORCEINLINE buffered_array(buffered_array&& other) noexcept
        {
            m_array = std::move(other.m_array);
        }

        OBLO_FORCEINLINE buffered_array& operator=(const buffered_array& other)
        {
            m_array = other.m_array;
            return *this;
        }

        OBLO_FORCEINLINE buffered_array& operator=(buffered_array&& other) noexcept
        {
            m_array = std::move(other.m_array);
            return *this;
        }

        OBLO_FORCEINLINE decltype(auto) begin()
        {
            return m_array.begin();
        }

        OBLO_FORCEINLINE decltype(auto) end()
        {
            return m_array.end();
        }

        OBLO_FORCEINLINE decltype(auto) begin() const
        {
            return m_array.begin();
        }

        OBLO_FORCEINLINE decltype(auto) end() const
        {
            return m_array.end();
        }

        OBLO_FORCEINLINE void reserve(usize n)
        {
            m_array.reserve(n);
        }

        OBLO_FORCEINLINE void resize(usize n)
        {
            m_array.resize(n);
        }

        OBLO_FORCEINLINE void resize(usize n, const T& value)
        {
            m_array.resize(n, value);
        }

        OBLO_FORCEINLINE T* data()
        {
            return m_array.data();
        }

        OBLO_FORCEINLINE const T* data() const
        {
            return m_array.data();
        }

        OBLO_FORCEINLINE bool empty() const
        {
            return m_array.empty();
        }

        OBLO_FORCEINLINE usize size() const
        {
            return m_array.size();
        }

        template <typename... TArgs>
        OBLO_FORCEINLINE T& emplace_back(TArgs&&... args)
        {
            return m_array.emplace_back(std::forward<TArgs>(args)...);
        }

        OBLO_FORCEINLINE auto push_back(const T& value)
        {
            return m_array.push_back(value);
        }

        OBLO_FORCEINLINE auto push_back(T&& value)
        {
            return m_array.push_back(std::move(value));
        }

        OBLO_FORCEINLINE void pop_back()
        {
            return m_array.pop_back();
        }

        OBLO_FORCEINLINE T& front()
        {
            return m_array.front();
        }

        OBLO_FORCEINLINE T& back()
        {
            return m_array.back();
        }

        OBLO_FORCEINLINE const T& front() const
        {
            return m_array.front();
        }

        OBLO_FORCEINLINE const T& back() const
        {
            return m_array.back();
        }

        OBLO_FORCEINLINE const T& operator[](usize index) const
        {
            return m_array[index];
        }

        OBLO_FORCEINLINE T& operator[](usize index)
        {
            return m_array[index];
        }

        template <typename... TArgs>
        OBLO_FORCEINLINE auto insert(TArgs&&... args)
        {
            return m_array.insert(std::forward<TArgs>(args)...);
        }

        template <typename... TArgs>
        OBLO_FORCEINLINE auto assign(TArgs&&... args)
        {
            return m_array.assign(std::forward<TArgs>(args)...);
        }

        OBLO_FORCEINLINE void clear()
        {
            m_array.clear();
        }

    private:
        stack_fallback_allocator<(sizeof(T) * N), alignof(T)> m_allocator;
        dynamic_array<T> m_array{&m_allocator};
    };
}