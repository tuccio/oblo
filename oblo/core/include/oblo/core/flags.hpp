#pragma once

#include <oblo/core/platform/compiler.hpp>
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

        template <typename E>
        consteval u32 choose_flags_size()
        {
            if constexpr (requires { u32(E::enum_max); })
            {
                return u32(E::enum_max);
            }
            else
            {
                return 0u;
            }
        }

        template <u32 Size>
        using flags_underlying_type = decltype(choose_underlying_type<Size>());
    }

    template <typename E, u32 Size = detail::choose_flags_size<E>()>
    struct flags
    {
    public:
        static_assert(Size > 0);
        using type = detail::flags_underlying_type<Size>;

    public:
        constexpr flags() = default;
        OBLO_FORCEINLINE constexpr flags(E value);
        constexpr flags(const flags&) noexcept = default;
        constexpr flags& operator=(const flags&) noexcept = default;

        OBLO_FORCEINLINE constexpr flags operator~() const;

        OBLO_FORCEINLINE constexpr flags& operator|=(E rhs);
        OBLO_FORCEINLINE constexpr flags& operator&=(E rhs);
        OBLO_FORCEINLINE constexpr flags& operator^=(E rhs);

        OBLO_FORCEINLINE constexpr flags& operator|=(flags<E> rhs);
        OBLO_FORCEINLINE constexpr flags& operator&=(flags<E> rhs);
        OBLO_FORCEINLINE constexpr flags& operator^=(flags<E> rhs);

        OBLO_FORCEINLINE constexpr bool is_empty() const noexcept;
        OBLO_FORCEINLINE constexpr bool is_full() const noexcept;

        OBLO_FORCEINLINE constexpr type data() const noexcept;

        OBLO_FORCEINLINE constexpr void set(E e) noexcept;
        OBLO_FORCEINLINE constexpr void unset(E e) noexcept;
        OBLO_FORCEINLINE constexpr void assign(E e, bool v) noexcept;

        OBLO_FORCEINLINE constexpr bool contains(E e) const noexcept;

        constexpr auto operator<=>(const flags&) const = default;

        OBLO_FORCEINLINE constexpr E find_first() const noexcept;

        OBLO_FORCEINLINE constexpr static type as_flag(E e) noexcept;

    public:
        type storage{};
    };

    template <typename E, u32 Size>
    constexpr flags<E, Size>::flags(E value) : storage{as_flag(value)}
    {
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size> flags<E, Size>::operator~() const
    {
        constexpr auto allMask = (type(1) << Size) - 1;
        flags f;
        f.storage = allMask & ~storage;
        return f;
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
    constexpr bool flags<E, Size>::is_empty() const noexcept
    {
        return storage == type{0};
    }

    template <typename E, u32 Size>
    constexpr bool flags<E, Size>::is_full() const noexcept
    {
        constexpr auto all = ~flags{};
        return storage == all.storage;
    }

    template <typename E, u32 Size>
    constexpr flags<E, Size>::type flags<E, Size>::data() const noexcept
    {
        return storage;
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
    constexpr void flags<E, Size>::assign(E e, bool v) noexcept
    {
        storage = (storage & ~as_flag(e)) | (type{v} << type(e));
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
    concept flags_enum = std::is_enum_v<E>&&
        requires()
    {
        {u32(E::enum_max)};
    };

    template <flags_enum E>
    constexpr flags<E> operator|(flags<E> lhs, E rhs) noexcept
    {
        lhs |= rhs;
        return lhs;
    }

    template <flags_enum E>
    constexpr flags<E> operator&(flags<E> lhs, E rhs) noexcept
    {
        lhs &= rhs;
        return lhs;
    }

    template <flags_enum E>
    constexpr flags<E> operator^(flags<E> lhs, E rhs) noexcept
    {
        lhs ^= rhs;
        return lhs;
    }

    template <flags_enum E>
    constexpr flags<E> operator|(E lhs, E rhs) noexcept
    {
        return flags{lhs} | rhs;
    }

    template <flags_enum E>
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