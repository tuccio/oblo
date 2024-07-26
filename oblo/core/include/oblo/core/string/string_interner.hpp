#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_view.hpp>

namespace oblo
{
    class string;

    class string_interner
    {
    public:
        static constexpr u32 MaxStringLength{255};

    public:
        string_interner() = default;
        string_interner(const string_interner&) = delete;
        string_interner(string_interner&&) noexcept;
        string_interner& operator=(const string_interner&) = delete;
        string_interner& operator=(string_interner&&) noexcept;
        ~string_interner();

        void init(u32 estimatedStringsCount);
        void shutdown();

        h32<string> get_or_add(string_view str);
        h32<string> get(string_view str) const;

        cstring_view str(h32<string> handle) const;
        const char* c_str(h32<string> handle) const;

    private:
        struct impl;
        impl* m_impl{nullptr};
    };
}