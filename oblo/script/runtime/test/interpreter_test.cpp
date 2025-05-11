#include <gtest/gtest.h>

#include <oblo/script/interpreter.hpp>
#include <oblo/script/module.hpp>

namespace oblo::script
{
    TEST(script_test, add_u32)
    {
        script::module m;

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
}