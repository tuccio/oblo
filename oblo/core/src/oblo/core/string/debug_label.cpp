#include <oblo/core/string/debug_label.hpp>

#include <oblo/core/string/format.hpp>
#include <oblo/core/utility.hpp>

#include <format>

namespace oblo
{
    debug_label::debug_label(string_view str)
    {
        const auto len = min<std::size_t>(str.size(), MaxLength);

        std::memcpy(m_label, str.data(), str.size());
        m_label[len] = '\0';
    }

    debug_label::debug_label(std::source_location loc)
    {
        const string_view path{loc.file_name()};
        const auto offset = path.find_last_of("\\/");

        string_view file;

        if (offset != string_view::npos)
        {
            file = path.substr(offset + 1);
        }
        else
        {
            file = path;
        }

        const auto endIt = std::format_to_n(m_label, MaxLength, "{}:{}", file, loc.line());
        *endIt.out = '\0';
    }
}