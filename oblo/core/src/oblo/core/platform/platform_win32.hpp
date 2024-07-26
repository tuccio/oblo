#pragma once

#ifdef _WIN32

    #include <oblo/core/debug.hpp>
    #include <oblo/core/string/string_view.hpp>

    #include <utf8cpp/utf8.h>

namespace oblo::win32
{
    static constexpr u32 MaxPath{260};

    inline wchar_t* convert_path(string_view in, wchar_t (&out)[MaxPath])
    {
        auto* end = utf8::unchecked::utf8to16(in.begin(), in.end(), out);
        OBLO_ASSERT(end < out + MaxPath);

        *end = {};

        return end + 1;
    }
}

#endif