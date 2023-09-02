#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/types.hpp>

#include <bit>
#include <compare>
#include <span>

namespace oblo
{
    struct uuid
    {
        u8 data[16];

        constexpr auto operator<=>(const uuid&) const = default;

        bool is_nil() const noexcept
        {
            return *this == uuid{};
        }

        enum class format_error
        {
            unexpected_format,
            buffer_too_small,
        };

        constexpr void format_to(std::span<char, 36> buffer) const;

        static constexpr auto parse(std::span<const char, 36> buffer);
        static constexpr auto parse(std::span<const char> buffer);

        constexpr bool parse_from(std::span<const char, 36> buffer);
        constexpr bool parse_from(std::span<const char> buffer);
    };

    constexpr void uuid::format_to(std::span<char, 36> buffer) const
    {
        constexpr auto push_chars = [](u8 byte, auto& out)
        {
            static constexpr char lookup[] =
                {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

            *out = lookup[(byte >> 8) & 0xF];
            ++out;

            *out = lookup[byte & 0xF];
            ++out;
        };

        auto it = buffer.begin();
        push_chars(data[0], it);
        push_chars(data[1], it);
        push_chars(data[2], it);
        push_chars(data[3], it);

        *it++ = '-';

        push_chars(data[4], it);
        push_chars(data[5], it);

        *it++ = '-';

        push_chars(data[6], it);
        push_chars(data[7], it);

        *it++ = '-';

        push_chars(data[8], it);
        push_chars(data[9], it);

        *it++ = '-';

        push_chars(data[10], it);
        push_chars(data[11], it);
        push_chars(data[12], it);
        push_chars(data[13], it);
        push_chars(data[14], it);
        push_chars(data[15], it);
    }

    constexpr auto uuid::parse(std::span<const char, 36> buffer)
    {
        using result_type = expected<uuid, uuid::format_error>;

        constexpr auto parse_chars = [](u8& byte, auto& in)
        {
            constexpr auto convert_char = [](char c) -> int
            {
                if (c >= '0' && c <= '9')
                {
                    return c - '0';
                }

                if (c >= 'a' && c <= 'f')
                {
                    return 10 + c - 'a';
                }

                if (c >= 'A' && c <= 'F')
                {
                    return 10 + c - 'A';
                }

                return -1;
            };

            const auto hi = convert_char(*in++);
            const auto lo = convert_char(*in++);

            if (hi < 0 || lo < 0)
            {
                return false;
            }

            byte = u8((hi << 8) | lo);

            return true;
        };

        auto it = buffer.begin();

        uuid result;
        auto& data = result.data;

        if (!parse_chars(data[0], it) || !parse_chars(data[1], it) || !parse_chars(data[2], it) ||
            !parse_chars(data[3], it))
        {
            return result_type{format_error::unexpected_format};
        }

        if (*it++ != '-')
        {
            return result_type{format_error::unexpected_format};
        }

        if (!parse_chars(data[4], it) || !parse_chars(data[5], it))
        {
            return result_type{format_error::unexpected_format};
        }

        if (*it++ != '-')
        {
            return result_type{format_error::unexpected_format};
        }

        if (!parse_chars(data[6], it) || !parse_chars(data[7], it))
        {
            return result_type{format_error::unexpected_format};
        }

        if (*it++ != '-')
        {
            return result_type{format_error::unexpected_format};
        }

        if (!parse_chars(data[8], it) || !parse_chars(data[9], it))
        {
            return result_type{format_error::unexpected_format};
        }

        if (*it++ != '-')
        {
            return result_type{format_error::unexpected_format};
        }

        if (!parse_chars(data[10], it) || !parse_chars(data[11], it) || !parse_chars(data[12], it) ||
            !parse_chars(data[13], it) || !parse_chars(data[14], it) || !parse_chars(data[15], it))
        {
            return result_type{format_error::unexpected_format};
        }

        return result_type{result};
    }

    constexpr auto uuid::parse(std::span<const char> buffer)
    {
        using result_type = expected<uuid, uuid::format_error>;

        if (buffer.size() < 36)
        {
            return result_type{format_error::buffer_too_small};
        }

        return uuid::parse(buffer.subspan<0, 36>());
    }

    constexpr bool uuid::parse_from(std::span<const char, 36> buffer)
    {
        const auto result = uuid::parse(buffer);

        if (result)
        {
            *this = *result;
            return true;
        }

        return false;
    }

    consteval uuid operator""_uuid(const char* str, size_t len)
    {
        const auto expected = uuid::parse(std::span{str, len});

        if (!expected)
        {
            throw "Failed to parse UUID";
        }

        return *expected;
    }
}

namespace std
{
    template <>
    struct hash<oblo::uuid>
    {
        constexpr auto operator()(const oblo::uuid& uuid) const noexcept
        {
            using namespace oblo;
            constexpr std::hash<u64> hash64{};

            struct u128
            {
                u64 hi;
                u64 lo;
            };

            const auto [hi, lo] = std::bit_cast<u128>(uuid);

            return hash_mix(hash64(hi), hash64(lo));
        }
    };
}