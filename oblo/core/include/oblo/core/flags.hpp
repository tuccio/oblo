#pragma once

#include <oblo/core/types.hpp>

#include <bit>
#include <compare>

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
    struct flags
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

        constexpr flags& operator|=(flags<E> rhs);
        constexpr flags& operator&=(flags<E> rhs);
        constexpr flags& operator^=(flags<E> rhs);

        constexpr bool is_empty() const noexcept
        {
            return storage == type{0};
        }

        constexpr type data() const noexcept
        {
            return storage;
        }

        constexpr void set(E e) noexcept;
        constexpr void unset(E e) noexcept;

        constexpr bool contains(E e) const noexcept;

        constexpr auto operator<=>(const flags&) const = default;

        constexpr E find_first() const noexcept;

        constexpr static type as_flag(E e) noexcept;

    public:
        type storage{};
    };

    template <typename E, u32 Size>
    constexpr flags<E, Size>::flags(E value) : storage{as_flag(value)}
    {
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator|=(E rhs)
    {
        storage |= as_flag(rhs);
        return *this;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator&=(E rhs)
    {
        storage &= as_flag(rhs);
        return *this;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator^=(E rhs)
    {
        storage ^= as_flag(rhs);
        return *this;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator|=(flags<E> rhs)
    {
        storage |= rhs.storage;
        return *this;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator&=(flags<E> rhs)
    {
        storage &= rhs.storage;
        return *this;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>& flags<E, Size>::operator^=(flags<E> rhs)
    {
        storage ^= rhs.storage;
        return *this;
    }

    template <typename E, u32 Size>
    constexpr void flags<E, Size>::set(E e) noexcept
    {
        storage |= as_flag(e);
    }

    template <typename E, u32 Size>
    constexpr void flags<E, Size>::unset(E e) noexcept
    {
        storage &= ~as_flag(e);
    }

    template <typename E, u32 Size>
    constexpr bool flags<E, Size>::contains(E e) const noexcept
    {
        return (storage & as_flag(e)) != 0;
    }

    template <typename E, u32 Size>
    constexpr E flags<E, Size>::find_first() const noexcept
    {
        const auto count = u32(std::countr_zero(storage));
        return static_cast<E>(count < Size ? count : Size);
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

    template <typename E>
    constexpr flags<E> operator|(E lhs, E rhs) noexcept
    {
        return flags{lhs} | rhs;
    }

    template <typename E>
    constexpr flags<E> operator&(E lhs, E rhs) noexcept
    {
        return flags{lhs} & rhs;
    }

    template <typename E>
    constexpr flags<E> operator^(E lhs, E rhs) noexcept
    {
        return flags{lhs} ^ rhs;
    }
}