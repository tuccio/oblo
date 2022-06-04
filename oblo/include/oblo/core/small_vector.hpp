#pragma once

#include <oblo/core/stack_allocator.hpp>
#include <vector>

namespace oblo
{
    template <typename T, std::size_t N>
    class small_vector : public std::pmr::vector<T>
    {
    public:
        small_vector()
        {
            *this = std::pmr::vector<T>{m_allocator};
            std::pmr::vector<T>::reserve(N);
        }

        small_vector(const small_vector&) = delete;
        small_vector(small_vector&&) = delete;
        small_vector& operator=(const small_vector&) = delete;
        small_vector& operator=(small_vector&&) = delete;

    private:
        using std::pmr::vector<T>::operator=;

    private:
        stack_allocator<sizeof(T) * N, alignof(T)> m_allocator;
    };
}