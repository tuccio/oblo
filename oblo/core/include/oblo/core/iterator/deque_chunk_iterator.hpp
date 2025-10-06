#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/platform/compiler.hpp>

#include <iterator>
#include <span>

namespace oblo
{
    template <typename T>
    class deque_chunk_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::span<T>;
        using reference = value_type;
        using pointer = value_type;
        using difference_type = ptrdiff;
        using size_type = usize;

        explicit deque_chunk_iterator(deque_iterator<T> it, usize dequeSize) : m_it{it}, m_dequeSize{dequeSize} {}

        deque_chunk_iterator() = default;
        deque_chunk_iterator(const deque_chunk_iterator&) = default;
        deque_chunk_iterator(deque_chunk_iterator&&) noexcept = default;

        deque_chunk_iterator& operator=(const deque_chunk_iterator&) = default;
        deque_chunk_iterator& operator=(deque_chunk_iterator&&) noexcept = default;

        OBLO_FORCEINLINE bool operator==(const deque_chunk_iterator& other) const = default;

        OBLO_FORCEINLINE deque_chunk_iterator& operator++()
        {
            const usize contiguousElements = get_elements_in_chunk();
            m_it = std::next(m_it, contiguousElements);
            return *this;
        }

        OBLO_FORCEINLINE deque_chunk_iterator operator++(int)
        {
            const auto it = *this;
            ++*this;
            return it;
        }

        OBLO_FORCEINLINE reference operator*() const
        {
            const usize contiguousElements = get_elements_in_chunk();
            return std::span<T>{m_it.operator->(), contiguousElements};
        }

        OBLO_FORCEINLINE friend bool operator<(const deque_chunk_iterator& lhs, const deque_chunk_iterator& rhs)
        {
            return lhs.m_it.m_index < rhs.m_it.m_index;
        }

    private:
        usize get_elements_in_chunk() const
        {
            const usize indexInChunk = m_it.m_index % m_it.m_elementsPerChunk;
            const usize elementsInChunk = m_it.m_elementsPerChunk - indexInChunk;
            const usize remainingInDeque = m_dequeSize - m_it.m_index;

            return elementsInChunk < remainingInDeque ? elementsInChunk : remainingInDeque;
        }

    private:
        deque_iterator<T> m_it{};
        usize m_dequeSize{};
    };
}