#pragma once

#include <oblo/core/types.hpp>

namespace oblo::script
{
    enum class opcode : u16
    {
        /// @brief Returns from function, freeing the stack.
        ret,

        /// @brief Push a 32-bit value on the stack, setting the low 16 bits using the payload value.
        push32lo16,

        /// @brief Modifies the 32-bit value at top of the stack, setting the payload values as high 16 bits.
        or32hi16,

        /// @brief Copies function parameter at top of the stack, using [size, offset] as payload.
        parampso,

        /// @brief Push value at the top of the stack to return value, using [size, offset] as payload.
        retvpso,

        // Binary operations: consume operands on stack, push result.
        addf32,
        addi32,
        addu32,
        addf64,
        addi64,
        addu64,
    };
}