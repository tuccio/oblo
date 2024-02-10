#pragma once

#include <oblo/core/uuid.hpp>

#include <format>

template <>
struct std::formatter<oblo::uuid>
{
    template <typename FormatParseContext>
    auto parse(FormatParseContext& pc) const
    {
        return pc.end();
    }

    template <typename FormatContext>
    auto format(const oblo::uuid& uuid, FormatContext& fc) const
    {
        constexpr auto N = 36;
        char buffer[N];
        uuid.format_to(buffer);
        return std::format_to(fc.out(), "{}", string_view{buffer, N});
    }
};