#pragma once

#include <oblo/core/forward.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    i32 utf8_compare(string_view lhs, string_view rhs);
    void utf8_to_utf16(string_view src, dynamic_array<char16_t>& dst);
    void utf8_to_wide(string_view src, dynamic_array<wchar_t>& dst);
}
