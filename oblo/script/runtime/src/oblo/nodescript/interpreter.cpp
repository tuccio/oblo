#include <oblo/script/interpreter.hpp>

#include <oblo/core/unreachable.hpp>

namespace oblo::script
{
    void interpreter::init(u32 stackSize)
    {
        m_stackMemory = allocate_unique<byte[]>(stackSize);
        m_stackTop = m_stackMemory.get();
        m_stackMax = m_stackTop + stackSize;
        m_nextInstruction = 0;
        m_code.clear();
        m_callFrame.clear();

        // The first function is just a dummy for the invalid handle, we could consider using it for an entry point
        m_functions.assign(1, {});
    }

    void interpreter::load_module(const module& m)
    {
        const usize baseOffset = m_code.size();
        m_code.append(m.text.begin(), m.text.end());

        for (auto& f : m.functions)
        {
            const auto [it, inserted] = m_functionNames.emplace(f.id, m_functions.size32());

            if (inserted)
            {
                m_functions.push_back({
                    .paramsSize = f.paramsSize,
                    .returnSize = f.returnSize,
                    .address = baseOffset + f.textOffset,
                });
            }
        }
    }

    h32<function> interpreter::find_function(hashed_string_view name) const
    {
        const auto it = m_functionNames.find(name);
        return it == m_functionNames.end() ? h32<function>{} : h32<function>{it->second};
    }

    void interpreter::call_function(h32<function> f)
    {
        const auto fnInfo = m_functions[f.value];

        OBLO_ASSERT(used_stack_size() >= fnInfo.paramsSize);

        allocate_stack(fnInfo.returnSize);

        auto& callFrame = m_callFrame.push_back_default();
        callFrame.returnAddr = m_nextInstruction;
        callFrame.restoreStackPtr = m_stackTop;

        m_nextInstruction = fnInfo.address;

        run();

        finish_call();
    }

    u32 interpreter::used_stack_size() const
    {
        return u32(m_stackTop - m_stackMemory.get());
    }

    u32 interpreter::available_stack_size() const
    {
        return u32(m_stackMax - m_stackTop);
    }

    void interpreter::run()
    {
        OBLO_ASSERT(m_nextInstruction < m_code.size());

        while (true)
        {
            const auto& bytecode = m_code[m_nextInstruction];

            switch (bytecode.op)
            {
            case opcode::ret:
                return;

            case opcode::push32lo16: {
                u16 v;
                bytecode_payload::unpack_u16(bytecode.payload, v);
                push_u32(v);
            }

                ++m_nextInstruction;
                break;

            case opcode::retvpso: {
                u8 size, offset;
                bytecode_payload::unpack_2xu8(bytecode.payload, size, offset);

                // Maybe somehow add an assert to check that the range is within the return value part of the stack
                auto* const rvEnd = m_callFrame.back().restoreStackPtr;
                auto* const dst = rvEnd - offset;
                auto* const src = m_stackTop - size;

                std::memcpy(dst, src, size);
            }

                ++m_nextInstruction;
                break;

            case opcode::addu32: {
                const u32 lhs = read_u32(0);
                const u32 rhs = read_u32(sizeof(u32));
                const u32 r = lhs + rhs;
                pop(sizeof(u32));
                std::memcpy(m_stackTop - sizeof(u32), &r, sizeof(u32));
            }

                ++m_nextInstruction;
                break;

            default:
                unreachable();
            }
        }
    }

    byte* interpreter::allocate_stack(u32 size)
    {
        byte* const r = m_stackTop;
        m_stackTop += size;
        OBLO_ASSERT(m_stackTop <= m_stackMax);
        return r;
    }

    void interpreter::finish_call()
    {
        const auto& f = m_callFrame.back();

        m_nextInstruction = f.returnAddr;
        m_stackTop = f.restoreStackPtr;

        m_callFrame.pop_back();
    }

    u32 interpreter::read_u32(u32 stackOffset)
    {
        u32 r;
        std::memcpy(&r, m_stackTop - stackOffset - sizeof(u32), sizeof(u32));
        return r;
    }

    void interpreter::push_u32(u32 value)
    {
        byte* const dst = allocate_stack(sizeof(u32));
        std::memcpy(dst, &value, sizeof(u32));
    }

    void interpreter::pop(u32 stackSize)
    {
        m_stackTop -= stackSize;
        OBLO_ASSERT(m_stackTop >= m_stackMemory.get());
    }
}