#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_view.hpp>

#include <format>

template <>
struct std::formatter<oblo::string_view>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const oblo::string_view& view, std::format_context& ctx) const
    {
        auto&& outIt = ctx.out();

        for (auto c : view)
        {
            *outIt = c;
            ++outIt;
        }

        return outIt;
    }
};

template <>
struct std::formatter<oblo::cstring_view>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const oblo::cstring_view& view, std::format_context& ctx) const
    {
        auto&& outIt = ctx.out();

        for (auto c : view)
        {
            *outIt = c;
            ++outIt;
        }

        return outIt;
    }
};

template <>
struct std::formatter<oblo::hashed_string_view>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const oblo::hashed_string_view& view, std::format_context& ctx) const
    {
        auto&& outIt = ctx.out();

        for (auto c : view)
        {
            *outIt = c;
            ++outIt;
        }

        return outIt;
    }
};

template <>
struct std::formatter<oblo::string>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const oblo::string& str, std::format_context& ctx) const
    {
        auto&& outIt = ctx.out();

        for (auto c : str)
        {
            *outIt = c;
            ++outIt;
        }

        return outIt;
    }
};