#include <gtest/gtest.h>

#include <oblo/script/bytecode_module.hpp>
#include <oblo/script/interpreter.hpp>

namespace oblo
{
    TEST(script_test, add_u32)
    {
        bytecode_module m;

        // Performs 27 + 15
        m.text = {
            {opcode::push32lo16, bytecode_payload::pack_u16(15)},
            {opcode::push32lo16, bytecode_payload::pack_u16(27)},
            {opcode::addu32},
            {opcode::ret},
        };

        interpreter interp;

        interp.init(1u << 10);
        interp.load_module(m);

        ASSERT_EQ(interp.used_stack_size(), 0);
        interp.run();

        ASSERT_EQ(interp.used_stack_size(), sizeof(u32));

        const u32 r = interp.read_u32(0);
        ASSERT_EQ(r, 42);
    }

    TEST(script_test, sub_i32)
    {
        bytecode_module m;

        // Performs 15 - 27
        m.text = {
            {opcode::push32lo16, bytecode_payload::pack_u16(27)},
            {opcode::push32lo16, bytecode_payload::pack_u16(15)},
            {opcode::subi32},
            {opcode::ret},
        };

        interpreter interp;

        interp.init(1u << 10);
        interp.load_module(m);

        ASSERT_EQ(interp.used_stack_size(), 0);
        interp.run();

        ASSERT_EQ(interp.used_stack_size(), sizeof(i32));

        const u32 r = interp.read_i32(0);
        ASSERT_EQ(r, -12);
    }

    TEST(script_test, loop)
    {
        bytecode_module m;

        constexpr u16 B = 21;
        constexpr u16 N = 42;

        m.text = {
            {opcode::push32lo16, bytecode_payload::pack_u16(0)}, // result := 0
            {opcode::push32lo16, bytecode_payload::pack_u16(B)}, // i := B

            // Copy i to the top of the stack, because leu32 will consume it
            {opcode::pushcppso, bytecode_payload::pack_2xu8(sizeof(u32), 0)},
            {opcode::push32lo16, bytecode_payload::pack_u16(N)},

            {opcode::leu32},

            // Load the address to jump to
            {opcode::instraddr},
            // Increment address to jump at the end (i.e. the ret instruction)
            {opcode::incu32pu16, bytecode_payload::pack_u16(10)},

            {opcode::jnz32}, // Jump to end if N <= i

            // Copy result and i to the top of the stack
            {opcode::pushcppso, bytecode_payload::pack_2xu8(sizeof(u32) * 2, 0)},

            // Add them up
            {opcode::addu32},

            // Store the result
            {opcode::stru32pso, bytecode_payload::pack_2xu8(sizeof(u32), sizeof(u32))},

            // Increment i by 1
            {opcode::incu32poi, bytecode_payload::pack_2xu8(0, 1)},

            // Jump at the beginning of the loop
            {opcode::instraddr},
            {opcode::decu32pu16, bytecode_payload::pack_u16(10)},

            {opcode::jmp},

            {opcode::ret},
        };

        interpreter interp;

        interp.init(1u << 10);
        interp.load_module(m);

        ASSERT_EQ(interp.used_stack_size(), 0);
        interp.run();

        // We still have result and i at the top of the stack
        ASSERT_EQ(interp.used_stack_size(), 2 * sizeof(u32));

        constexpr u32 Expected = ((N * (N + 1)) / 2) - (B * (B + 1)) / 2 - (N - B);

        ASSERT_EQ(interp.read_u32(0), N);
        ASSERT_EQ(interp.read_u32(sizeof(u32)), Expected);
    }
}