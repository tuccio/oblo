#include <oblo/core/string/string_builder.hpp>

#include <oblo/core/string/string.hpp>

#include <utf8cpp/utf8/unchecked.h>

#include <filesystem>

namespace oblo
{
    template <typename T>
    T* find_end(T* b, T* e)
    {
        if (e != nullptr)
        {
            return e;
        }

        while (*b != '\0')
        {
            ++b;
        }

        return b;
    }

    string_builder& string_builder::append(const string& str)
    {
        return append(str.data(), str.data() + str.size());
    }

    string_builder& string_builder::append(const char* str, const char* end)
    {
        end = find_end(str, end);
        return append(string_view(str, end));
    }

    string_builder& string_builder::append(const wchar_t* str, const wchar_t* end)
    {
        end = find_end(str, end);

        m_buffer.pop_back();
        utf8::unchecked::utf16to8(str, end, std::back_inserter(m_buffer));
        ensure_null_termination();

        return *this;
    }

    string_builder& string_builder::append(const char8_t* str, const char8_t* end)
    {
        end = find_end(str, end);
        return append(string_view(str, end));
    }

    string_builder& string_builder::append(const char16_t* str, const char16_t* end)
    {
        end = find_end(str, end);

        m_buffer.pop_back();
        utf8::unchecked::utf16to8(str, end, std::back_inserter(m_buffer));
        ensure_null_termination();

        return *this;
    }

    namespace
    {
        constexpr bool is_path_separator(char c)
        {
#ifdef _WIN32
            return c == '\\' || c == '/';
#else
            return c == '/';
#endif
        }
    }

    string_builder& string_builder::append_path_separator()
    {
#ifdef _WIN32
        constexpr char separator = '\\';
#else
        constexpr char separator = '/';
#endif

        if (const auto len = size(); len > 0 && is_path_separator(m_buffer[len - 1]))
        {
            return *this;
        }

        return append(separator);
    }

    string_builder& string_builder::append_path(string_view str)
    {
        return append_path_separator().append(str);
    }

    string_builder& string_builder::make_absolute_path()
    {
        const auto sv = view();

        std::error_code ec;
        auto p = std::filesystem::absolute(std::u8string_view{sv.u8data(), sv.size()}, ec);

        return clear().append(p.native().data(), p.native().data() + p.native().size());
    }

    string_builder& string_builder::make_canonical_path()
    {
        const auto sv = view();

        std::error_code ec;
        auto p = std::filesystem::canonical(std::u8string_view{sv.u8data(), sv.size()}, ec);

        return clear().append(p.native().data(), p.native().data() + p.native().size());
    }

    string_builder& string_builder::trim_end()
    {
        utf8::unchecked::iterator it{m_buffer.end() - 1};
        const utf8::unchecked::iterator b{m_buffer.begin()};

        while (it != b)
        {
            const auto code32 = *it;

            if (code32 != 0 && !std::isspace(int(code32)))
            {
                ++it;
                break;
            }

            --it;
        }

        const auto newSize = it.base() - b.base();
        resize(newSize);

        return *this;
    }

    bool string_builder::operator==(const string_builder& other) const noexcept
    {
        return view() == other.view();
    }
}