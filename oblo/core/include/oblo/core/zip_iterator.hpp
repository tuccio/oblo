#pragma once

#include <iterator>
#include <tuple>

namespace oblo
{
    template <typename... Iterators>
    class zip_iterator
    {
    public:
        using iterator_tuple = std::tuple<Iterators...>;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = std::tuple<typename std::iterator_traits<Iterators>::value_type...>;
        using reference = std::tuple<typename std::iterator_traits<Iterators>::value_type&...>;
        using pointer = value_type*;
        using difference_type = std::ptrdiff_t;
        using size_type = std::size_t;

        template <typename... T>
        explicit zip_iterator(T&&... iterators)
            requires(std::is_constructible_v<Iterators, T> && ...)
            : m_iterators{std::forward<T>(iterators)...}
        {
        }

        zip_iterator() = default;
        zip_iterator(const zip_iterator&) = default;
        zip_iterator(zip_iterator&&) noexcept = default;

        zip_iterator& operator=(const zip_iterator&) = default;
        zip_iterator& operator=(zip_iterator&&) noexcept = default;

        bool operator==(const zip_iterator& other) const
        {
            return std::get<0>(other.m_iterators) == std::get<0>(m_iterators);
        }

        bool operator!=(const zip_iterator& other) const
        {
            return !(*this == other);
        }

        zip_iterator& operator++()
        {
            std::apply([](auto&... it) { (++it, ...); }, m_iterators);
            return *this;
        }

        zip_iterator operator++(int)
        {
            const auto it = *this;
            ++*this;
            return it;
        }

        zip_iterator& operator--()
        {
            std::apply([](auto&... it) { (--it, ...); }, m_iterators);
            return *this;
        }

        zip_iterator operator--(int)
        {
            const auto it = *this;
            --*this;
            return it;
        }

        reference operator*() const
        {
            return std::apply([](auto&... it) { return std::forward_as_tuple(*it...); }, m_iterators);
        }

        friend void swap(zip_iterator& lhs, zip_iterator& rhs)
        {
            std::apply(
                [&rhs](auto&... lhsIts)
                {
                    std::apply(
                        [&lhsIts...](auto&... rhsIts)
                        {
                            using std::swap;
                            (swap(lhsIts, rhsIts), ...);
                        },
                        rhs.m_iterators);
                },
                lhs.m_iterators);
        }

        friend void iter_swap(const zip_iterator& lhs, const zip_iterator& rhs)
        {
            std::apply(
                [&rhs](auto&... lhsIts)
                {
                    std::apply(
                        [&lhsIts...](auto&... rhsIts)
                        {
                            using std::swap;
                            (swap(*lhsIts, *rhsIts), ...);
                        },
                        rhs.m_iterators);
                },
                lhs.m_iterators);
        }

        friend bool operator<(const zip_iterator& lhs, const zip_iterator& rhs)
        {
            return std::get<0>(lhs.m_iterators) < std::get<0>(rhs.m_iterators);
        }

        zip_iterator& operator+=(size_type offset)
        {
            std::apply([offset](auto&... it) { ((it += offset), ...); }, m_iterators);
            return *this;
        }

        zip_iterator& operator-=(size_type offset)
        {
            std::apply([offset](auto&... it) { ((it -= offset), ...); }, m_iterators);
            return *this;
        }

        friend zip_iterator operator+(zip_iterator it, size_type offset)
        {
            it += offset;
            return it;
        }

        friend zip_iterator operator+(size_type offset, const zip_iterator& it)
        {
            return (it + offset);
        }

        friend zip_iterator operator-(zip_iterator it, size_type offset)
        {
            it -= offset;
            return it;
        }

        friend difference_type operator-(const zip_iterator& lhs, const zip_iterator& rhs)
        {
            return std::get<0>(lhs.m_iterators) - std::get<0>(rhs.m_iterators);
        }

        reference operator[](size_type offset) const
        {
            return *(*this + offset);
        }

        const iterator_tuple& get_iterator_tuple() const
        {
            return m_iterators;
        }

    private:
        iterator_tuple m_iterators;
    };

    template <typename... Iterators>
    zip_iterator(Iterators...) -> zip_iterator<Iterators...>;
}

namespace std
{
    template <typename... Iterators>
    void iter_swap(const oblo::zip_iterator<Iterators...>& lhs, const oblo::zip_iterator<Iterators...>& rhs)
    {
        iter_swap(lhs, rhs);
    }
}