#include <utf8cpp/utf8/unchecked.h>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo
{
    i32 utf8_compare(string_view lhs, string_view rhs)
    {
        auto it1 = lhs.begin();
        const auto end1 = lhs.end();

        auto it2 = rhs.begin();
        const auto end2 = rhs.end();

        while (it1 != end1 && it2 != end2)
        {
            const uint32_t cp1 = utf8::unchecked::next(it1);
            const uint32_t cp2 = utf8::unchecked::next(it2);

            if (cp1 < cp2)
            {
                return -1;
            }

            if (cp1 > cp2)
            {
                return 1;
            }
        }

        // If we get here, one string ended
        if (it1 == end1 && it2 == end2)
        {
            return 0;
        }

        if (it1 == end1)
        {
            return -1;
        }
        else
        {
            return 1;
        }
    }

    void utf8_to_utf16(string_view src, dynamic_array<char16_t>& dst)
    {
        utf8::unchecked::utf8to16(src.begin(), src.end(), std::back_inserter(dst));
    }

    void utf8_to_wide(string_view src, dynamic_array<wchar_t>& dst)
    {
        if constexpr (platform::is_windows())
        {
            static_assert(!platform::is_windows() || sizeof(wchar_t) == 2);
            utf8::unchecked::utf8to16(src.begin(), src.end(), std::back_inserter(dst));
        }
        else if constexpr (platform::is_linux())
        {
            static_assert(!platform::is_linux() || sizeof(wchar_t) == 4);
            utf8::unchecked::utf8to32(src.begin(), src.end(), std::back_inserter(dst));
        }
    }
}