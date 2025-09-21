#include <gtest/gtest.h>

#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/script/bytecode_module.hpp>
#include <oblo/script/interpreter.hpp>

namespace oblo
{
    TEST(script_test, add_u32)
    {
        bytecode_module m;

        // Performs 27 + 15
        m.text = {
            {bytecode_op::push_32lo16, bytecode_payload::pack_u16(15)},
            {bytecode_op::push_32lo16, bytecode_payload::pack_u16(27)},
            {bytecode_op::add_u32},
            {bytecode_op::ret},
        };

        interpreter interp;

        interp.init(1u << 10);
        interp.load_module(m);

        ASSERT_EQ(interp.used_stack_size(), 0);
        ASSERT_TRUE(interp.run());

        ASSERT_EQ(interp.used_stack_size(), sizeof(u32));

        const expected<u32, interpreter_error> r = interp.read_u32(0);
        ASSERT_TRUE(r);
        ASSERT_EQ(*r, 42);
    }

    TEST(script_test, sub_i32)
    {
        bytecode_module m;

        // Performs 15 - 27
        m.text = {
            {bytecode_op::push_32lo16, bytecode_payload::pack_u16(27)},
            {bytecode_op::push_32lo16, bytecode_payload::pack_u16(15)},
            {bytecode_op::sub_i32},
            {bytecode_op::ret},
        };

        interpreter interp;

        interp.init(1u << 10);
        interp.load_module(m);

        ASSERT_EQ(interp.used_stack_size(), 0);
        ASSERT_TRUE(interp.run());

        ASSERT_EQ(interp.used_stack_size(), sizeof(i32));

        const expected<i32, interpreter_error> r = interp.read_i32(0);
        ASSERT_TRUE(r);
        ASSERT_EQ(*r, -12);
    }

    TEST(script_test, loop)
    {
        bytecode_module m;

        constexpr u16 B = 21;
        constexpr u16 N = 42;

        m.text = {
            {bytecode_op::push_32lo16, bytecode_payload::pack_u16(0)}, // result := 0
            {bytecode_op::push_32lo16, bytecode_payload::pack_u16(B)}, // i := B

            // Copy i to the top of the stack, because leu32 will consume it
            {bytecode_op::push_copy_sizeoffset, bytecode_payload::pack_2xu8(sizeof(u32), 0)},
            {bytecode_op::push_32lo16, bytecode_payload::pack_u16(N)},

            {bytecode_op::le_u32},

            // Load the address to jump to
            {bytecode_op::push_current_instruction_address},
            // Increment address to jump at the end (i.e. the ret instruction)
            {bytecode_op::increment_u32_val, bytecode_payload::pack_u16(10)},

            {bytecode_op::jnz32}, // Jump to end if N <= i

            // Copy result and i to the top of the stack
            {bytecode_op::push_copy_sizeoffset, bytecode_payload::pack_2xu8(sizeof(u32) * 2, 0)},

            // Add them up
            {bytecode_op::add_u32},

            // Store the result
            {bytecode_op::store_32_sizeoffset, bytecode_payload::pack_2xu8(sizeof(u32), sizeof(u32) * 2)},

            // Increment i by 1
            {bytecode_op::increment_stackref_u32_offsetval, bytecode_payload::pack_2xu8(0, 1)},

            // Jump at the beginning of the loop
            {bytecode_op::push_current_instruction_address},
            {bytecode_op::decrement_u32_u16, bytecode_payload::pack_u16(10)},

            {bytecode_op::jmp},

            {bytecode_op::ret},
        };

        interpreter interp;

        interp.init(1u << 10);
        interp.load_module(m);

        ASSERT_EQ(interp.used_stack_size(), 0);
        ASSERT_TRUE(interp.run());

        // We still have result and i at the top of the stack
        ASSERT_EQ(interp.used_stack_size(), 2 * sizeof(u32));

        constexpr u32 Expected = ((N * (N + 1)) / 2) - (B * (B + 1)) / 2 - (N - B);

        const expected r1 = interp.read_u32(0);
        const expected r2 = interp.read_u32(sizeof(u32));

        ASSERT_TRUE(r1);
        ASSERT_TRUE(r2);

        ASSERT_EQ(*r1, N);
        ASSERT_EQ(*r2, Expected);
    }

    TEST(script_test, call_native)
    {
        bytecode_module m;

        m.readOnlyStrings = {"my_native_fun"};

        m.text = {
            {bytecode_op::call_api_static, bytecode_payload::pack_u16(0)},
            {bytecode_op::ret},
        };

        interpreter interp;

        interp.init(1u << 10);
        interp.load_module(m);
        interp.register_api("my_native_fun", [](interpreter& i) { i.push_u32(42); });

        ASSERT_EQ(interp.used_stack_size(), 0);
        ASSERT_TRUE(interp.run());

        ASSERT_EQ(interp.used_stack_size(), sizeof(u32));

        const expected<u32, interpreter_error> r = interp.read_u32(0);
        ASSERT_TRUE(r);
        ASSERT_EQ(*r, 42);
    }
}