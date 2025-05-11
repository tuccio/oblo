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

        /// @brief Copies values from somewhere on the stack to the top, using [size: u8, offset: u8] as payload.
        pushcppso,

        /// @brief Modifies the 32-bit value at top of the stack, setting the payload values as high 16 bits.
        or32hi16,

        // @brief Overwrite value on stack with the value on top of the stack, using [size: u8, offset: u8] as payload.
        stru32pso,

        /// @brief Push value at the top of the stack to return value, using [size: u8, offset: u8] as payload.
        retvpso,

        /// @brief Checks the 2 elements at the top of the stack for a == b. Pushes 1 if they are equal, 0 otherwise.
        equ32,
        eqi32 = equ32,

        /// @brief Checks the 2 elements at the top of the stack for a >= b. Pushes 1 if true, 0 otherwise.
        geu32,

        /// @brief Checks the 2 elements at the top of the stack for a <= b. Pushes 1 if true, 0 otherwise.
        leu32,

        /// @brief Unconditional jump to the address at the top of the stack.
        jmp,

        /// @brief Jumps to the address at the top of the stack if the second argument is non-zero.
        jnz32,

        /// @brief Jumps to the address at the top of the stack if the second argument is zero.
        jz32,

        /// @brief Pushes the current instruction address at the top of the stack.
        instraddr,

        /// @brief Increments the value at the top of the stack with the u16 value in the payload.
        incu32pu16,
        /// @brief Increments the value at the top of the stack with the u16 value in the payload.
        inci32pu16,

        /// @brief Increments the value at the given offset with payload [offset: u8, amount: u8].
        incu32poi,

        /// @brief Increments the value at the top of the stack with the u16 value in the payload.
        decu32pu16,
        /// @brief Increments the value at the top of the stack with the u16 value in the payload.
        deci32pu16,

        // Binary operations: consume operands on stack, push result.
        addf32,
        addi32,
        addu32,
        addf64,
        addi64,
        addu64,

        subf32,
        subi32,
        subu32,
        subf64,
        subi64,
        subu64,
    };
}