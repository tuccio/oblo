#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/stack_allocator.hpp>

namespace oblo
{
    template <typename T, usize N>
    class small_vector
    {
    public:
        small_vector()
        {
            m_array.reserve(N);
        }

        small_vector(allocator* fallback) : m_allocator{fallback}
        {
            m_array.reserve(N);
        }

        small_vector(const std::initializer_list<T> initializer)
        {
            m_array.reserve(N);
            m_array = initializer;
        }

        small_vector(const small_vector&) = delete;
        small_vector(small_vector&&) = delete;
        small_vector& operator=(const small_vector&) = delete;
        small_vector& operator=(small_vector&&) = delete;

        decltype(auto) begin()
        {
            return m_array.begin();
        }

        decltype(auto) end()
        {
            return m_array.end();
        }

        decltype(auto) begin() const
        {
            return m_array.begin();
        }

        decltype(auto) end() const
        {
            return m_array.end();
        }

        void reserve(usize n)
        {
            m_array.reserve(n);
        }

        void resize(usize n)
        {
            m_array.resize(n);
        }

        void resize(usize n, const T& value)
        {
            m_array.resize(n, value);
        }

        T* data()
        {
            return m_array.data();
        }

        const T* data() const
        {
            return m_array.data();
        }

        bool empty() const
        {
            return m_array.empty();
        }

        usize size() const
        {
            return m_array.size();
        }

        template <typename... TArgs>
        T& emplace_back(TArgs&&... args)
        {
            return m_array.emplace_back(std::forward<TArgs>(args)...);
        }

        auto push_back(const T& value)
        {
            return m_array.push_back(value);
        }

        auto push_back(T&& value)
        {
            return m_array.push_back(std::move(value));
        }

        void pop_back()
        {
            return m_array.pop_back();
        }

        const T& front() const
        {
            return m_array.front();
        }

        const T& back() const
        {
            return m_array.back();
        }

        const T& operator[](usize index) const
        {
            return m_array[index];
        }

        T& operator[](usize index)
        {
            return m_array[index];
        }

        template <typename... TArgs>
        auto insert(TArgs&&... args)
        {
            return m_array.insert(std::forward<TArgs>(args)...);
        }

        template <typename... TArgs>
        auto assign(TArgs&&... args)
        {
            return m_array.assign(std::forward<TArgs>(args)...);
        }

    private:
        stack_fallback_allocator<(sizeof(T) * N), alignof(T)> m_allocator;
        dynamic_array<T> m_array{&m_allocator};
    };
}