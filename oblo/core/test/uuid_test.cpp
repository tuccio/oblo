#include <gtest/gtest.h>

#include <oblo/core/uuid.hpp>

#include <algorithm>

namespace oblo
{
    TEST(uuid, formatting)
    {
        constexpr uuid nil{};

        char buffer[36];
        const string_view uuidStr{buffer, 36};

        std::fill(std::begin(buffer), std::end(buffer), 'Z');
        nil.format_to(buffer);

        ASSERT_EQ(uuidStr, "00000000-0000-0000-0000-000000000000");

        constexpr uuid prog{
            .data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef}};

        prog.format_to(buffer);
        ASSERT_EQ(uuidStr, "01234567-89ab-cdef-0123-456789abcdef");

        constexpr auto parseResult = uuid::parse("01234567-89ab-cdef-0123-456789abcdef");

        ASSERT_TRUE(parseResult);
        ASSERT_EQ(*parseResult, prog);

        constexpr uuid literal = "01234567-89ab-cdef-0123-456789abcdef"_uuid;
        ASSERT_EQ(literal, prog);
    }
}