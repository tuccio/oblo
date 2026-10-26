#pragma once

#include <oblo/core/forward.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

#include <initializer_list>
#include <iterator>

namespace oblo
{
    template <typename Container>
    concept span_container = requires(Container& c) {
        c.data();
        c.size();
    };

    template <typename Container>
    using span_container_pointer_type = decltype(std::declval<Container&>().data());
    template <typename Container>
    using span_container_value_type = std::remove_pointer_t<decltype(std::declval<Container&>().data())>;

    template <typename T>
    class span
    {
    public:
        static constexpr usize npos = ~usize{};

        using element_type = T;
        using value_type = T;
        using size_type = usize;
        using difference_type = isize;
        using pointer = T*;
        using reference = T&;
        using iterator = T*;
        using const_iterator = const T*;

        constexpr span() noexcept = default;

        constexpr span(T* begin, usize size) noexcept;

        template <typename U>
            requires std::is_convertible_v<U*, T*>
        constexpr span(U* begin, U* end) noexcept;

        template <usize N>
        constexpr span(T (&array)[N]) noexcept;

        template <usize N>
        constexpr span(fixed_span<T, N> other) noexcept;

        template <span_container Container>
            requires std::is_convertible_v<span_container_pointer_type<Container>, T*>
        constexpr span(Container& container) noexcept;

        template <typename U>
            requires std::is_convertible_v<U*, T*>
        constexpr span(span<U> other) noexcept;

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

        constexpr span subspan(size_type pos, size_type count = npos) const noexcept;

        constexpr span<T> first(usize count) const noexcept;
        constexpr span<T> last(usize count) const noexcept;

        constexpr reverse_iterator<T*> rbegin() const noexcept;
        constexpr reverse_iterator<T*> rend() const noexcept;

    private:
        T* m_begin{};
        usize m_size{};
    };

    template <typename T>
    constexpr span<T>::span(T* begin, usize size) noexcept : m_begin{begin}, m_size{size}
    {
    }

    template <typename T>
    template <typename U>
        requires std::is_convertible_v<U*, T*>
    constexpr span<T>::span(U* begin, U* end) noexcept : m_begin{begin}, m_size{static_cast<usize>(end - begin)}
    {
    }

    template <typename T>
    template <usize N>
    constexpr span<T>::span(T (&array)[N]) noexcept : m_begin{array}, m_size{N}
    {
    }

    template <typename T>
    template <usize N>
    constexpr span<T>::span(fixed_span<T, N> other) noexcept : m_begin{other.data()}, m_size{other.size()}
    {
    }

    template <typename T>
    template <span_container Container>
        requires std::is_convertible_v<span_container_pointer_type<Container>, T*>
    constexpr span<T>::span(Container& container) noexcept : m_begin{container.data()}, m_size{container.size()}
    {
    }

    template <typename T>
    template <typename U>
        requires std::is_convertible_v<U*, T*>
    constexpr span<T>::span(span<U> other) noexcept : m_begin{other.data()}, m_size{other.size()}
    {
    }

    template <typename T>
    constexpr T* span<T>::data() const noexcept
    {
        return m_begin;
    }

    template <typename T>
    constexpr usize span<T>::size() const noexcept
    {
        return m_size;
    }

    template <typename T>
    constexpr u32 span<T>::size32() const noexcept
    {
        return narrow_cast<u32>(m_size);
    }

    template <typename T>
    constexpr usize span<T>::size_bytes() const noexcept
    {
        return m_size * sizeof(T);
    }

    template <typename T>
    constexpr u32 span<T>::size_bytes32() const noexcept
    {
        return narrow_cast<u32>(m_size * sizeof(T));
    }

    template <typename T>
    constexpr bool span<T>::empty() const noexcept
    {
        return m_size == 0;
    }

    template <typename T>
    constexpr T* span<T>::begin() const noexcept
    {
        return m_begin;
    }

    template <typename T>
    constexpr T* span<T>::end() const noexcept
    {
        return m_begin + m_size;
    }

    template <typename T>
    constexpr reverse_iterator<T*> span<T>::rbegin() const noexcept
    {
        return reverse_iterator<T*>{begin()};
    }

    template <typename T>
    constexpr reverse_iterator<T*> span<T>::rend() const noexcept
    {
        return reverse_iterator<T*>{end()};
    }

    template <typename T>
    constexpr T& span<T>::operator[](usize index) const noexcept
    {
        return m_begin[index];
    }

    template <typename T>
    constexpr T& span<T>::front() const noexcept
    {
        return m_begin[0];
    }

    template <typename T>
    constexpr T& span<T>::back() const noexcept
    {
        return m_begin[m_size - 1];
    }

    template <typename T>
    constexpr span<T> span<T>::subspan(size_type pos, size_type count) const noexcept
    {
        OBLO_ASSERT(pos <= m_size);
        return {m_begin + pos, clamp_offset(m_size, pos, count)};
    }

    template <typename T>
    constexpr span<T> span<T>::first(usize count) const noexcept
    {
        OBLO_ASSERT(count <= m_size);
        return {m_begin, count};
    }

    template <typename T>
    constexpr span<T> span<T>::last(usize count) const noexcept
    {
        OBLO_ASSERT(count <= m_size);
        return {m_begin + (m_size - count), count};
    }

    // Utility functions

    template <typename T>
    constexpr span<const T> make_span_initializer(std::initializer_list<T> list)
    {
        return {list.begin(), list.end()};
    }

    template <typename T>
    constexpr span<const byte> as_bytes(span<T> s)
    {
        return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
    }

    template <typename T>
    constexpr span<byte> as_writable_bytes(span<T> s)
    {
        return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
    }

    // CTAD

    template <typename T, usize N>
    span(T (&array)[N]) -> span<T>;

    template <span_container Container>
    span(Container&) -> span<span_container_value_type<Container>>;

    template <span_container Container>
    span(const Container&) -> span<span_container_value_type<const Container>>;
}
