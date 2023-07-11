#pragma once

#include <memory_resource>
#include <vector>

namespace oblo
{
    template <typename T, std::size_t N>
    class small_vector
    {
    public:
        small_vector() : m_vector{&m_resource}
        {
            m_vector.reserve(N);
        }

        small_vector(const std::initializer_list<T> initializer) : m_vector{&m_resource}
        {
            m_vector.reserve(N);
            m_vector.assign(initializer.begin(), initializer.end());
        }

        small_vector(const small_vector&) = delete;
        small_vector(small_vector&&) = delete;
        small_vector& operator=(const small_vector&) = delete;
        small_vector& operator=(small_vector&&) = delete;

        decltype(auto) begin()
        {
            return m_vector.begin();
        }

        decltype(auto) end()
        {
            return m_vector.end();
        }

        decltype(auto) begin() const
        {
            return m_vector.begin();
        }

        decltype(auto) end() const
        {
            return m_vector.end();
        }

        void resize(std::size_t n)
        {
            m_vector.resize(n);
        }

        void resize(std::size_t n, const T& value)
        {
            m_vector.resize(n, value);
        }

        T* data()
        {
            return m_vector.data();
        }

        const T* data() const
        {
            return m_vector.data();
        }

        std::size_t size() const
        {
            return m_vector.size();
        }

        template <typename... TArgs>
        decltype(auto) emplace_back(TArgs&&... args)
        {
            return m_vector.emplace_back(std::forward<TArgs>(args)...);
        }

        decltype(auto) push_back(const T& value)
        {
            return m_vector.push_back(value);
        }

        decltype(auto) push_back(T&& value)
        {
            return m_vector.push_back(std::move(value));
        }

        const T& operator[](std::size_t index) const
        {
            return m_vector[index];
        }

        T& operator[](std::size_t index)
        {
            return m_vector[index];
        }

        template <typename... TArgs>
        decltype(auto) insert(TArgs&&... args)
        {
            return m_vector.insert(std::forward<TArgs>(args)...);
        }

        template <typename... TArgs>
        decltype(auto) assign(TArgs&&... args)
        {
            return m_vector.assign(std::forward<TArgs>(args)...);
        }

    private:
        alignas(T) char m_buffer[sizeof(T) * N];
        std::pmr::monotonic_buffer_resource m_resource{m_buffer, sizeof(T) * N};
        std::pmr::vector<T> m_vector;
    };
}