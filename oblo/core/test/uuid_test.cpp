#include <gtest/gtest.h>

#include <oblo/core/uuid.hpp>

#include <algorithm>

namespace oblo
{
    TEST(uuid, formatting)
    {
        constexpr uuid nil{};

        char buffer[36];
        const std::string_view uuidStr{buffer, 36};

        std::fill(std::begin(buffer), std::end(buffer), 'Z');
        nil.format_to(buffer);

        ASSERT_EQ(uuidStr, "00000000-0000-0000-0000-000000000000");

        constexpr uuid prog{.data = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf}};

        prog.format_to(buffer);
        ASSERT_EQ(uuidStr, "00010203-0405-0607-0809-0a0b0c0d0e0f");

        constexpr auto parseResult = uuid::parse("00010203-0405-0607-0809-0a0b0c0d0e0f");

        ASSERT_TRUE(parseResult);
        ASSERT_EQ(*parseResult, prog);

        constexpr uuid literal = "00010203-0405-0607-0809-0a0b0c0d0e0f"_uuid;
        ASSERT_EQ(literal, prog);
    }
}