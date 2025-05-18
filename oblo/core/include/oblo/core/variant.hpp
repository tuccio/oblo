#pragma once

#include <oblo/core/meta_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>

#include <array>

namespace oblo
{
    template <typename... T>
    class variant;

    template <typename F, typename... T>
    decltype(auto) visit(F&& f, variant<T...>& v);

    template <typename F, typename... T>
    decltype(auto) visit(F&& f, const variant<T...>& v);

    template <typename... T>
    class variant
    {
        static_assert(sizeof...(T) > 0);

        template <typename First, typename...>
        using first_type = First;

    public:
        using index_type = u8;
        static constexpr index_type types_count = index_type(sizeof...(T));

    public:
        variant() : m_index{0}
        {
            new (m_buffer) first_type<T...>{};
        }

        variant(const variant& other)
        {
            oblo::visit(
                [this]<typename U>(const U& o) OBLO_FORCEINLINE_LAMBDA
                {
                    new (m_buffer) U{o};
                    m_index = index_of<U>();
                },
                other);
        }

        variant(variant&& other) noexcept
        {
            oblo::visit(
                [this]<typename U>(U& o) OBLO_FORCEINLINE_LAMBDA
                {
                    new (m_buffer) U{std::move(o)};
                    m_index = index_of<U>();
                },
                other);
        }

        ~variant()
        {
            if constexpr ((!std::is_trivially_destructible_v<T> || ...))
            {
                visit([]<typename V>(V& o) OBLO_FORCEINLINE_LAMBDA { o.~V(); });
            }
        }

        template <typename U>
        variant(U&& o)
            requires(index_of<U>() < types_count)
        {
            new (m_buffer) U{std::forward<U>(o)};
            m_index = index_of<U>();
        }

        variant& operator=(const variant& other)
        {
            oblo::visit([this]<typename U>(U& o) OBLO_FORCEINLINE_LAMBDA { emplace<U>(o); }, other);
            return *this;
        }

        variant& operator=(variant&& other) noexcept
        {
            oblo::visit([this]<typename U>(U& o) OBLO_FORCEINLINE_LAMBDA { emplace<U>(std::move(o)); }, other);
            return *this;
        }

        template <typename U, typename... Args>
        U& emplace(Args&&... args)
        {
            visit([]<typename V>(V& o) OBLO_FORCEINLINE_LAMBDA { o.~V(); });
            U* const r = new (m_buffer) U{std::forward<Args>(args)...};
            m_index = index_of<U>();
            return *r;
        }

        index_type index() const noexcept
        {
            return m_index;
        }

        template <typename U>
        bool is() const noexcept
            requires(index_of<U>() < types_count)
        {
            return m_index == index_of<U>();
        }

        template <typename U>
        U& as() noexcept
            requires(index_of<U>() < types_count)
        {
            OBLO_ASSERT(is<U>());
            return reinterpret_cast<U&>(m_buffer);
        }

        template <typename U>
        U& as() const noexcept
            requires(index_of<U>() < types_count)
        {
            OBLO_ASSERT(is<U>());
            return reinterpret_cast<const U&>(m_buffer);
        }

        template <typename F>
        decltype(auto) visit(F&& f)
        {
            static constexpr std::array visitors{+[](byte* buffer, std::add_lvalue_reference_t<F> c)
                                                      OBLO_FORCEINLINE_LAMBDA
                { return c(*reinterpret_cast<T*>(buffer)); }...};

            const auto& visitor = visitors[m_index];
            return visitor(m_buffer, f);
        }

        template <typename F>
        decltype(auto) visit(F&& f) const
        {
            static constexpr std::array visitors{+[](const byte* buffer, std::add_lvalue_reference_t<F> c)
                                                      OBLO_FORCEINLINE_LAMBDA
                { return c(*reinterpret_cast<const T*>(buffer)); }...};

            const auto& visitor = visitors[m_index];
            return visitor(m_buffer, f);
        }

    private:
        template <typename U>
        static consteval index_type index_of() noexcept
        {
            constexpr std::array types{meta_id<T>...};
            constexpr auto target = meta_id<U>;

            for (index_type i = 0; i < types_count; ++i)
            {
                if (target == types[i])
                {
                    return i;
                }
            }

            return types_count;
        }

    private:
        index_type m_index;
        alignas(max(alignof(T)...)) byte m_buffer[max(sizeof(T)...)];
    };

    template <typename F, typename... T>
    decltype(auto) visit(F&& f, variant<T...>& v)
    {
        return v.visit(std::forward<F>(f));
    }

    template <typename F, typename... T>
    decltype(auto) visit(F&& f, const variant<T...>& v)
    {
        return v.visit(std::forward<F>(f));
    }
}