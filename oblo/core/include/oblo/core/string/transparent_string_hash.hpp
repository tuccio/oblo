#pragma once

#include <oblo/core/hash.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo
{
    struct transparent_string_hash
    {
        using is_transparent = void;

        usize operator()(cstring_view str) const
        {
            return hash<cstring_view>{}(str);
        }

        usize operator()(string_view str) const
        {
            return hash<string_view>{}(str);
        }

        usize operator()(const string& str) const
        {
            return hash<string>{}(str);
        }

        usize operator()(const string_builder& str) const
        {
            return hash<string_view>{}(str.as<string_view>());
        }
    };
}