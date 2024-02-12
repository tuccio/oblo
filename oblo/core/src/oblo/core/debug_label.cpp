#include <oblo/core/debug_label.hpp>

#include <oblo/core/utility.hpp>

#include <format>

namespace oblo
{
    debug_label debug_label::from_source_location(std::source_location loc)
    {
        return debug_label{loc};
    }

    debug_label::debug_label(std::string_view str)
    {
        const auto len = min<std::size_t>(str.size(), MaxLength);

        std::memcpy(m_label, str.data(), str.size());
        m_label[len] = '\0';
    }

    debug_label::debug_label(std::source_location loc)
    {
        const std::string_view path{loc.file_name()};
        const auto offset = path.find_last_of("\\/");

        std::string_view file;

        if (offset != std::string_view::npos)
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