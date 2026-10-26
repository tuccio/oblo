#pragma once

#include <oblo/core/forward.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

#include <iterator>

namespace oblo
{
    template <typename T, usize N>
    class fixed_span
    {
    public:
        static constexpr usize extent = N;

        using element_type = T;
        using value_type = T;
        using size_type = usize;
        using difference_type = isize;
        using pointer = T*;
        using reference = T&;
        using iterator = T*;
        using const_iterator = const T*;

        constexpr fixed_span() noexcept = default;

        constexpr fixed_span(T* begin) noexcept;

        template <typename U>
            requires std::is_convertible_v<U*, T*>
        constexpr fixed_span(U* begin, U* end) noexcept;

        template <typename U>
            requires std::is_convertible_v<U*, T*>
        constexpr fixed_span(U* begin, usize size) noexcept;

        template <usize M>
            requires(M == N)
        constexpr fixed_span(T (&array)[M]) noexcept;

        template <typename U, usize M>
            requires(std::is_convertible_v<U*, T*> && M == N)
        constexpr fixed_span(const fixed_span<U, M>& other) noexcept;

        template <typename Container>
            requires std::is_convertible_v<span_container_pointer_type<Container>, T*> && requires(Container& c) {
                { c.size() } -> std::convertible_to<usize>;
            }
        constexpr fixed_span(Container& container) noexcept;

        constexpr T* data() const noexcept;

        constexpr usize size() const noexcept;
        constexpr usize size_bytes() const noexcept;

        constexpr u32 size32() const noexcept;
        constexpr u32 size_bytes32() const noexcept;

        constexpr bool empty() const noexcept;

        constexpr T* begin() const noexcept;
        constexpr T* end() const noexcept;

        constexpr T& operator[](usize index) const noexcept;

        constexpr T& front() const noexcept;
        constexpr T& back() const noexcept;

        constexpr fixed_span subspan(size_type pos, size_type count = N) const noexcept;

        constexpr fixed_span<T, N> first(usize count) const noexcept;
        constexpr fixed_span<T, N> last(usize count) const noexcept;

        constexpr reverse_iterator<T*> rbegin() const noexcept;
        constexpr reverse_iterator<T*> rend() const noexcept;

    private:
        T* m_begin{};
    };

    template <typename T, usize N>
    constexpr fixed_span<T, N>::fixed_span(T* begin) noexcept : m_begin{begin}
    {
    }

    template <typename T, usize N>
    template <typename U>
        requires std::is_convertible_v<U*, T*>
    constexpr fixed_span<T, N>::fixed_span(U* begin, U* end) noexcept : m_begin{begin}
    {
        OBLO_ASSERT(static_cast<usize>(end - begin) == N);
    }

    template <typename T, usize N>
    template <typename U>
        requires std::is_convertible_v<U*, T*>
    constexpr fixed_span<T, N>::fixed_span(U* begin, usize size) noexcept : m_begin{begin}
    {
        OBLO_ASSERT(size == N);
    }

    template <typename T, usize N>
    template <usize M>
        requires(M == N)
    constexpr fixed_span<T, N>::fixed_span(T (&array)[M]) noexcept : m_begin{array}
    {
    }

    template <typename T, usize N>
    template <typename U, usize M>
        requires(std::is_convertible_v<U*, T*> && M == N)
    constexpr fixed_span<T, N>::fixed_span(const fixed_span<U, M>& other) noexcept : m_begin{other.data()}
    {
    }

    template <typename T, usize N>
    template <typename Container>
        requires std::is_convertible_v<span_container_pointer_type<Container>, T*> && requires(Container& c) {
            { c.size() } -> std::convertible_to<usize>;
        }
    constexpr fixed_span<T, N>::fixed_span(Container& container) noexcept : m_begin{container.data()}
    {
        OBLO_ASSERT(container.size() == N);
    }

    template <typename T, usize N>
    constexpr T* fixed_span<T, N>::data() const noexcept
    {
        return m_begin;
    }

    template <typename T, usize N>
    constexpr usize fixed_span<T, N>::size() const noexcept
    {
        return N;
    }

    template <typename T, usize N>
    constexpr usize fixed_span<T, N>::size_bytes() const noexcept
    {
        return N * sizeof(T);
    }

    template <typename T, usize N>
    constexpr u32 fixed_span<T, N>::size32() const noexcept
    {
        return narrow_cast<u32>(N);
    }

    template <typename T, usize N>
    constexpr u32 fixed_span<T, N>::size_bytes32() const noexcept
    {
        return narrow_cast<u32>(N * sizeof(T));
    }

    template <typename T, usize N>
    constexpr bool fixed_span<T, N>::empty() const noexcept
    {
        return N == 0;
    }

    template <typename T, usize N>
    constexpr T* fixed_span<T, N>::begin() const noexcept
    {
        return m_begin;
    }

    template <typename T, usize N>
    constexpr T* fixed_span<T, N>::end() const noexcept
    {
        return m_begin + N;
    }

    template <typename T, usize N>
    constexpr reverse_iterator<T*> fixed_span<T, N>::rbegin() const noexcept
    {
        return reverse_iterator<T*>{end()};
    }

    template <typename T, usize N>
    constexpr reverse_iterator<T*> fixed_span<T, N>::rend() const noexcept
    {
        return reverse_iterator<T*>{begin()};
    }

    template <typename T, usize N>
    constexpr T& fixed_span<T, N>::operator[](usize index) const noexcept
    {
        OBLO_ASSERT(index < N);
        return m_begin[index];
    }

    template <typename T, usize N>
    constexpr T& fixed_span<T, N>::front() const noexcept
    {
        OBLO_ASSERT(N > 0);
        return m_begin[0];
    }

    template <typename T, usize N>
    constexpr T& fixed_span<T, N>::back() const noexcept
    {
        OBLO_ASSERT(N > 0);
        return m_begin[N - 1];
    }

    template <typename T, usize N>
    constexpr fixed_span<T, N> fixed_span<T, N>::subspan(size_type pos, size_type count) const noexcept
    {
        OBLO_ASSERT(pos <= N);
        OBLO_ASSERT(count <= N - pos);
        OBLO_ASSERT(count == N);

        return {m_begin + pos};
    }

    template <typename T, usize N>
    constexpr fixed_span<T, N> fixed_span<T, N>::first(usize count) const noexcept
    {
        OBLO_ASSERT(count == N);
        return {m_begin};
    }

    template <typename T, usize N>
    constexpr fixed_span<T, N> fixed_span<T, N>::last(usize count) const noexcept
    {
        OBLO_ASSERT(count == N);
        return {m_begin + (N - count)};
    }

    // CTAD

    template <typename T, usize N>
    fixed_span(T (&array)[N]) -> fixed_span<T, N>;

    template <span_container Container>
    fixed_span(Container&)
        -> fixed_span<span_container_value_type<Container>, std::extent_v<span_container_value_type<Container>>>;
}