#pragma once

#include <compare>

namespace oblo
{
    template <unsigned int N>
    struct fixed_string
    {
        constexpr fixed_string(const char (&string)[N])
        {
            for (unsigned int i = 0; i != N; ++i)
            {
                this->string[i] = string[i];
            }
        }

        constexpr auto operator<=>(const fixed_string&) const = default;

        char string[N]{};
    };

    template <unsigned int N>
    fixed_string(const char (&)[N]) -> fixed_string<N>;
}