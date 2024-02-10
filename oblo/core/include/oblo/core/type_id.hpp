#pragma once

#include <oblo/core/types.hpp>

#include <compare>
#include <string_view>

namespace oblo
{
    namespace detail
    {
        template <typename T>
        struct static_type_name
        {
            static constexpr std::string_view get()
            {
#if defined(__clang__)
                constexpr auto offset = sizeof("static std::string_view oblo::detail::static_type_name<") - 1;

                constexpr std::string_view str{__PRETTY_FUNCTION__};
                constexpr auto end = str.find_first_of('[') - sizeof(">::get()");

                return str.substr(offset, end - offset);
#elif defined(_MSC_VER)
                constexpr auto offset = sizeof("class std::basic_string_view<char,struct std::char_traits<char> > "
                                               "__cdecl oblo::detail::static_type_name<") -
                    1;

                constexpr std::string_view str{__FUNCSIG__};

                return str.substr(offset, str.find_last_of('>') - offset);
#else
#error "Unsupported compiler"
#endif
            }
        };
    }

    template <typename T>
    constexpr std::string_view get_qualified_type_name()
    {
        return detail::static_type_name<T>::get();
    }

    struct type_id
    {
        std::string_view name;

        constexpr auto operator<=>(const type_id&) const = default;
    };

    template <typename T>
    constexpr type_id get_type_id()
    {
        return {.name = get_qualified_type_name<T>()};
    }
}

namespace std
{
    template <>
    struct hash<oblo::type_id>
    {
        auto operator()(const oblo::type_id& typeId) const noexcept
        {
            return hash<std::string_view>{}(typeId.name);
        }
    };
}