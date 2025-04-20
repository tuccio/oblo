#pragma once

#include <oblo/core/uuid.hpp>

#include <format>

template <>
struct std::formatter<oblo::uuid>
{
    constexpr auto parse(std::format_parse_context& ctx) const
    {
        return ctx.begin();
    }

    auto format(const oblo::uuid& uuid, std::format_context& ctx) const
    {
        constexpr auto N = 36;
        char buffer[N];
        uuid.format_to(buffer);

        auto&& outIt = ctx.out();

        for (auto c : buffer)
        {
            *outIt = c;
            ++outIt;
        }

        return outIt;
    }
};