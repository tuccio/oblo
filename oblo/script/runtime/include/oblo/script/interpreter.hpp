#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/script/bytecode_module.hpp>

#include <unordered_map>

namespace oblo
{
    struct script_function;
    class interpreter;

    using script_api_fn = function_ref<void(interpreter&)>;

    enum class interpreter_error : u8;

    class interpreter
    {
    public:
        interpreter();
        interpreter(const interpreter&) = delete;
        interpreter(interpreter&&) noexcept = delete;

        ~interpreter();

        interpreter& operator=(const interpreter&) = delete;
        interpreter& operator=(interpreter&&) noexcept = delete;

        void init(u32 stackSize);

        void load_module(const bytecode_module& m);

        void register_api(string_view name, script_api_fn fn);

        h32<script_function> find_function(hashed_string_view name) const;

        expected<void, interpreter_error> call_function(h32<script_function> f);

        expected<f32, interpreter_error> read_f32(u32 stackOffset) const;
        expected<u32, interpreter_error> read_u32(u32 stackOffset) const;
        expected<i32, interpreter_error> read_i32(u32 stackOffset) const;

        void push_u32(u32 value);

        expected<void, interpreter_error> pop(u32 stackSize);

        u32 used_stack_size() const;
        u32 available_stack_size() const;

        expected<void, interpreter_error> run();

    private:
        using address_offset = u32;
        using instruction_idx = u32;

        struct call_frame
        {
            instruction_idx returnAddr;
            byte* restoreStackPtr;
        };

        struct function_info
        {
            u32 paramsSize;
            u32 returnSize;
            instruction_idx address;
        };

        struct read_only_string
        {
            string data;
            usize hash;
        };

    private:
        byte* allocate_stack(u32 size);
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
        std::unordered_map<string, u32, transparent_string_hash, std::equal_to<>> m_functionNames;
        std::unordered_map<string, script_api_fn, transparent_string_hash, std::equal_to<>> m_apiFunctions;
    };

    enum class interpreter_error : u8
    {
        unknown_instruction,
        stack_underflow,
    };
}