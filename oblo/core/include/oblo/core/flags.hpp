#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    namespace detail
    {
        template <u32 Size>
        consteval auto choose_underlying_type()
        {
            if constexpr (Size < 8)
            {
                return u8{};
            }

            else if constexpr (Size < 16)
            {
                return u16{};
            }

            else if constexpr (Size < 32)
            {
                return u32{};
            }

            else if constexpr (Size < 64)
            {
                return u64{};
            }
        }

        template <u32 Size>
        using flags_underlying_type = decltype(choose_underlying_type<Size>());
    }

    template <typename E, u32 Size = u32(E::enum_max)>
    class flags
    {
    public:
        using type = detail::flags_underlying_type<Size>;

    public:
        constexpr flags() = default;
        constexpr flags(E value);
        constexpr flags(const flags&) noexcept = default;
        constexpr flags& operator=(const flags&) noexcept = default;

        constexpr flags& operator|=(E rhs);
        constexpr flags& operator&=(E rhs);
        constexpr flags& operator^=(E rhs);

        bool is_empty() const noexcept
        {
            m_storage == type{0};
        }

        constexpr auto operator<=>(const flags&) const = default;

    private:
        constexpr static type as_flag(E e) noexcept;

    private:
        type m_storage{};
    };

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator|=(E rhs)
    {
        m_storage |= as_flag(rhs);
        return *this;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator&=(E rhs)
    {
        m_storage &= as_flag(rhs);
        return *this;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator^=(E rhs)
    {
        m_storage ^= as_flag(rhs);
        return *this;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>::type flags<E, Size>::as_flag(E e) noexcept
    {
        return type{1} << type(e);
    }

    template <typename E>
    constexpr flags<E> operator|(flags<E> lhs, E rhs) noexcept
    {
        lhs |= rhs;
        return lhs;
    }

    template <typename E>
    constexpr flags<E> operator&(flags<E> lhs, E rhs) noexcept
    {
        lhs &= rhs;
        return lhs;
    }

    template <typename E>
    constexpr flags<E> operator^(flags<E> lhs, E rhs) noexcept
    {
        lhs ^= rhs;
        return lhs;
    }
}