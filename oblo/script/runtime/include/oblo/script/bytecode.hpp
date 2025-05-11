#pragma once

#include <oblo/script/operations.hpp>

namespace oblo::script
{
    struct bytecode_payload
    {
        static constexpr bytecode_payload pack_u16(u16 v)
        {
            return {.data = v};
        }

        static constexpr bytecode_payload pack_2xu8(u8 a, u8 b)
        {
            return {.data = u16((u16{a} << 8) | b)};
        }

        static constexpr void unpack_u16(const bytecode_payload& p, u16& v)
        {
            v = p.data;
        }

        static constexpr void unpack_2xu8(const bytecode_payload& p, u8& a, u8& b)
        {
            a = u8(p.data >> 8);
            b = u8(p.data);
        }

        u16 data;
    };

    struct bytecode_instruction
    {
        opcode op;
        bytecode_payload payload;
    };
}