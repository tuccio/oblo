#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    enum class bytecode_op : u16
    {
        /// @brief Returns from function, freeing the stack.
        ret,

        /// @brief Push a 32-bit value on the stack, setting the low 16 bits using the payload value.
        push_32lo16,

        /// @brief Copies values from somewhere on the stack to the top, using [size: u8, offset: u8] as payload.
        push_copy_sizeoffset,

        /// @brief Pushes a string view from the read-only strings, using [read-only string id: u16].
        push_read_only_string_view,

        /// @brief Modifies the 32-bit value at top of the stack, setting the payload values as high 16 bits.
        or_32hi16,

        // @brief Overwrite value on stack with the value on top of the stack, using [size: u8, offset: u8] as payload.
        store_32_sizeoffset,

        /// @brief Push value at the top of the stack to return value, using [size: u8, offset: u8] as payload.
        push_value_sizeoffset,

        /// @brief Checks the 2 elements at the top of the stack for a == b. Pushes 1 if they are equal, 0 otherwise.
        eq_u32,
        eq_i32 = eq_u32,

        /// @brief Checks the 2 elements at the top of the stack for a >= b. Pushes 1 if true, 0 otherwise.
        ge_u32,

        /// @brief Checks the 2 elements at the top of the stack for a <= b. Pushes 1 if true, 0 otherwise.
        le_u32,

        /// @brief Unconditional jump to the address at the top of the stack.
        jmp,

        /// @brief Jumps to the address at the top of the stack if the second argument is non-zero.
        jnz32,

        /// @brief Jumps to the address at the top of the stack if the second argument is zero.
        jz32,

        /// @brief Pushes the current instruction address at the top of the stack.
        push_current_instruction_address,

        /// @brief Pushes a data ref to the N bytes at the top of the stack. The size is passed as u16 payload.
        push_stack_top_ref,

        /// @brief Tags the data ref at the top of the stack with a name. The payload is the u16 read-only string id.
        /// Consumes the data ref.
        tag_data_ref_static,

        /// @brief Pushes a previously tagged stack address. The payload is the u16 read-only string id.
        push_tagged_data_ref_static,

        /// @brief Increments the value at the top of the stack with the u16 value in the payload.
        increment_u32_val,
        /// @brief Increments the value at the top of the stack with the u16 value in the payload.
        increment_i32_val,

        /// @brief Increments the value at the given offset with payload [offset: u8, amount: u8].
        increment_stackref_u32_offsetval,

        /// @brief Increments the value at the top of the stack with the u16 value in the payload.
        decrement_u32_u16,
        /// @brief Increments the value at the top of the stack with the u16 value in the payload.
        decrement_i32_val,

        // Binary operations: consume operands on stack, push result.
        add_f32,
        add_i32,
        add_u32,
        add_f64,
        add_i64,
        add_u64,

        sub_f32,
        sub_i32,
        sub_u32,
        sub_f64,
        sub_i64,
        sub_u64,

        /// @brief Calls a native API function, looking it up by name using a string in the data segment identified by
        /// the u16 id in the payload.
        call_api_static,
    };

    consteval u32 script_string_ref_size()
    {
        return 8u;
    }

    consteval u32 script_data_ref_size()
    {
        return 8u;
    }
}