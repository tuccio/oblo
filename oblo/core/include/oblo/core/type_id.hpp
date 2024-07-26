#pragma once

#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    namespace detail
    {
        template <typename T>
        struct static_type_name
        {
            static constexpr string_view get()
            {
#if defined(__clang__)
                constexpr auto offset = sizeof("static string_view oblo::detail::static_type_name<") - 1;

                constexpr string_view str{__PRETTY_FUNCTION__};
                constexpr auto end = str.find_first_of('[') - sizeof(">::get()");

                return str.substr(offset, end - offset);
#elif defined(_MSC_VER)
                constexpr auto offset = sizeof("class oblo::string_view __cdecl oblo::detail::static_type_name<") - 1;

                constexpr string_view str{__FUNCSIG__};

                return str.substr(offset, str.find_last_of('>') - offset);
#else
    #error "Unsupported compiler"
#endif
            }
        };
    }

    template <typename T>
    constexpr hashed_string_view get_qualified_type_name()
    {
        return hashed_string_view{detail::static_type_name<T>::get()};
    }

    struct type_id
    {
        hashed_string_view name;

        constexpr bool operator==(const type_id&) const noexcept = default;
    };

    template <typename T>
    constexpr type_id get_type_id()
    {
        return {.name = get_qualified_type_name<T>()};
    }

    template <>
    struct hash<type_id>
    {
        auto operator()(const type_id& typeId) const noexcept
        {
            return typeId.name.hash();
        }
    };
}

namespace std
{
    template <typename T>
    struct hash;

    template <>
    struct hash<oblo::type_id>
    {
        auto operator()(const oblo::type_id& typeId) const noexcept
        {
            return typeId.name.hash();
        }
    };
}