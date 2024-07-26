#pragma once

#include <format>

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>

template <>
struct std::formatter<oblo::string_view> : std::formatter<std::string_view>
{
    auto format(const oblo::string_view& view, std::format_context& ctx) const
    {
        const std::string_view sv{view.data(), view.size()};
        return std::formatter<std::string_view>::format(sv, ctx);
    }
};

template <>
struct std::formatter<oblo::cstring_view> : std::formatter<std::string_view>
{
    auto format(const oblo::cstring_view& view, std::format_context& ctx) const
    {
        const std::string_view sv{view.data(), view.size()};
        return std::formatter<std::string_view>::format(sv, ctx);
    }
};

template <>
struct std::formatter<oblo::hashed_string_view> : std::formatter<std::string_view>
{
    auto format(const oblo::hashed_string_view& view, std::format_context& ctx) const
    {
        const std::string_view sv{view.data(), view.size()};
        return std::formatter<std::string_view>::format(sv, ctx);
    }
};