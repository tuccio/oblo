#pragma once

#include <oblo/core/types.hpp>

#include <compare>

namespace oblo
{
    template <u32 N>
    struct fixed_string
    {
        constexpr fixed_string(const char (&string)[N])
        {
            for (u32 i = 0; i != N; ++i)
            {
                this->string[i] = string[i];
            }
        }

        constexpr u32 size() const
        {
            return N - 1;
        }

        constexpr const char* c_str() const
        {
            return string;
        }

        constexpr const char* data() const
        {
            return string;
        }

        constexpr auto operator<=>(const fixed_string&) const = default;

        char string[N]{};
    };

    template <unsigned int N>
    fixed_string(const char (&)[N]) -> fixed_string<N>;
}