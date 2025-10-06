#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/script/bytecode_module.hpp>

#include <functional>
#include <unordered_map>

namespace oblo
{
    struct script_function;
    class interpreter;

    enum class interpreter_error : u8;

    using script_api_fn = std::function<expected<void, interpreter_error>(interpreter&)>;

    class interpreter
    {
    public:
        using stack_address = u32;

    public:
        interpreter();
        interpreter(const interpreter&) = delete;
        interpreter(interpreter&&) noexcept = delete;

        ~interpreter();

        interpreter& operator=(const interpreter&) = delete;
        interpreter& operator=(interpreter&&) noexcept = delete;

        void init(u32 stackSize);

        void load_module(const bytecode_module& m);

        void register_api(string_view name, script_api_fn fn, u32 paramsSize, u32 returnSize);

        h32<script_function> find_function(hashed_string_view name) const;

        expected<void, interpreter_error> call_function(h32<script_function> f);

        /// @brief It can be used by native functions to set the return value.
        /// @param data The value to return from the function, it needs to match the function return size.
        expected<void, interpreter_error> set_function_return(std::span<const byte> data);

        expected<f32, interpreter_error> read_f32(u32 stackOffset) const;
        expected<u32, interpreter_error> read_u32(u32 stackOffset) const;
        expected<i32, interpreter_error> read_i32(u32 stackOffset) const;

        expected<string_view, interpreter_error> get_string_view(u32 stackOffset) const;
        expected<std::span<const byte>, interpreter_error> get_data_view(u32 stackOffset) const;

        void push_u32(u32 value);

        /// @brief Pushes a sequence of bytes onto the data stack.
        /// @param data A span representing the sequence of bytes to be pushed onto the stack.
        /// @return An expected value containing the stack address on success, or an interpreter error on failure.
        expected<stack_address, interpreter_error> push_data(std::span<const byte> data);

        expected<void, interpreter_error> push_data_view(stack_address address, u32 size);
        expected<void, interpreter_error> push_string_view(stack_address address, u32 size);

        expected<void, interpreter_error> pop(u32 stackSize);

        u32 used_stack_size() const;
        u32 available_stack_size() const;

        expected<void, interpreter_error> run();

        void reset_execution();

    private:
        using address_offset = u32;
        using instruction_idx = u32;

        struct call_frame;
        struct function_info;
        struct read_only_string;
        struct runtime_tag;
        struct native_function;

    private:
        byte* allocate_stack(u32 size);

        expected<void, interpreter_error> begin_call(u32 paramsSize, u32 returnSize, instruction_idx returnAddr);
        void finish_call();

    private:
        unique_ptr<byte[]> m_stackMemory;
        byte* m_stackTop{};
        instruction_idx m_nextInstruction{};
        dynamic_array<bytecode_instruction> m_code;
        dynamic_array<call_frame> m_callFrame;
        byte* m_stackMax{};
        dynamic_array<function_info> m_functions;
        dynamic_array<read_only_string> m_readOnlyStrings;
        deque<runtime_tag> m_runtimeTags;
        std::unordered_map<string, u32, transparent_string_hash, std::equal_to<>> m_functionNames;
        std::unordered_map<string, native_function, transparent_string_hash, std::equal_to<>> m_apiFunctions;
    };

    enum class interpreter_error : u8
    {
        unknown_instruction,
        stack_overflow,
        stack_underflow,
        invalid_arguments,
        invalid_string,
        invalid_tag,
        unknown_function,
    };
}