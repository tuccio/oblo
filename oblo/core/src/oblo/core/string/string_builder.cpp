#include <oblo/core/string/string_builder.hpp>

#include <oblo/core/string/string.hpp>

#include <utf8cpp/utf8.h>

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
        utf8::utf16to8(str, end, std::back_inserter(m_buffer));
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
        utf8::utf16to8(str, end, std::back_inserter(m_buffer));
        ensure_null_termination();

        return *this;
    }

    string_builder& string_builder::append_path_separator()
    {
#ifdef _WIN32
        constexpr char separator = '\\';
#else
        constexpr char separator = '/';
#endif

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

    bool string_builder::operator==(const string_builder& other) const noexcept
    {
        return view() == other.view();
    }
}