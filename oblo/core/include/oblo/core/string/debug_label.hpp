#pragma once

#include <oblo/core/string/string_view.hpp>

#include <source_location>

namespace oblo
{
    class debug_label
    {
    public:
        constexpr debug_label() : m_label{} {}

        template <unsigned N>
        constexpr debug_label(const char (&name)[N])
        {
            unsigned i = 0;

            for (; i < N - 1 && i < Size - 1; ++i)
            {
                m_label[i] = name[i];
            }

            m_label[i] = '\0';
        }

        debug_label(string_view str);
        debug_label(std::source_location loc);

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