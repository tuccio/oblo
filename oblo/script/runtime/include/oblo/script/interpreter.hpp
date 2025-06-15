#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/script/bytecode_module.hpp>

#include <unordered_map>

namespace oblo
{
    struct function;

    class interpreter
    {
    public:
        void init(u32 stackSize);

        void load_module(const bytecode_module& m);

        h32<function> find_function(hashed_string_view name) const;

        void call_function(h32<function> f);

        u32 read_u32(u32 stackOffset) const;
        i32 read_i32(u32 stackOffset) const;

        void push_u32(u32 value);

        void pop(u32 stackSize);

        u32 used_stack_size() const;
        u32 available_stack_size() const;

        void run();

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
        std::unordered_map<string, u32, transparent_string_hash, std::equal_to<>> m_functionNames;
    };
}