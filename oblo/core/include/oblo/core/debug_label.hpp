#pragma once

#include <source_location>
#include <string_view>

namespace oblo
{
    class debug_label
    {
    public:
        static debug_label from_source_location(std::source_location loc = std::source_location::current());

        constexpr debug_label() : m_label{} {}

        debug_label(std::string_view str);
        explicit debug_label(std::source_location loc);

        constexpr debug_label(const debug_label&) = default;
        constexpr debug_label(debug_label&&) noexcept = default;

        constexpr debug_label& operator=(const debug_label&) = default;
        constexpr debug_label& operator=(debug_label&&) noexcept = default;

        constexpr const char* get() const;

    private:
        static constexpr auto MaxLength{63u};
        static constexpr auto Size{MaxLength + 1u};
        char m_label[Size];
    };

    constexpr const char* oblo::debug_label::get() const
    {
        return m_label;
    }
}