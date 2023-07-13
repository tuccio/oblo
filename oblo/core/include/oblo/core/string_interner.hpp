#pragma once

#include <oblo/core/handle.hpp>

#include <string_view>

namespace oblo
{
    struct string;

    class string_interner
    {
    public:
        static constexpr u32 MaxStringLength{255};

    public:
        string_interner() = default;
        string_interner(const string_interner&) = delete;
        string_interner(string_interner&&) noexcept = delete;
        string_interner& operator=(const string_interner&) = delete;
        string_interner& operator=(string_interner&&) noexcept = delete;
        ~string_interner();

        void init(u32 estimatedStringsCount);
        void shutdown();

        handle<string> get_or_add(std::string_view str);
        handle<string> get(std::string_view str) const;

        std::string_view str(handle<string> handle) const;
        const char* c_str(handle<string> handle) const;

    private:
        struct impl;
        impl* m_impl{nullptr};
    };
}