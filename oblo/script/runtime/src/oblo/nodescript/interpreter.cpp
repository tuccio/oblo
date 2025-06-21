#include <oblo/script/interpreter.hpp>

#include <oblo/core/unreachable.hpp>

namespace oblo
{
    namespace
    {
#if OBLO_DEBUG
        constexpr bool g_DebugStackMemory = true;
#else
        constexpr bool g_DebugStackMemory = false;
#endif

        template <typename T>
        OBLO_FORCEINLINE byte* stack_addr(byte* stackTop, u32 stackOffset)
        {
            return stackTop - stackOffset - sizeof(T);
        }

        template <typename T>
        OBLO_FORCEINLINE T read_stack(byte* stackTop, u32 stackOffset)
        {
            T r;
            std::memcpy(&r, stackTop - stackOffset - sizeof(T), sizeof(T));
            return r;
        }

        template <typename T>
        OBLO_FORCEINLINE [[nodiscard]] byte* binary_add(byte* stackTop)
        {
            const T lhs = read_stack<T>(stackTop, 0);
            const T rhs = read_stack<T>(stackTop, sizeof(T));
            const T r = lhs + rhs;

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(T));

            // Return new stack top
            return resPtr + sizeof(T);
        }

        template <typename T>
        OBLO_FORCEINLINE [[nodiscard]] byte* binary_sub(byte* stackTop)
        {
            const T lhs = read_stack<T>(stackTop, 0);
            const T rhs = read_stack<T>(stackTop, sizeof(T));
            const T r = lhs - rhs;

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(T));

            // Return new stack top
            return resPtr + sizeof(T);
        }

        template <typename T>
        OBLO_FORCEINLINE [[nodiscard]] byte* compare_ge(byte* stackTop)
        {
            using result_t = u32;

            const T lhs = read_stack<T>(stackTop, 0);
            const T rhs = read_stack<T>(stackTop, sizeof(T));
            const result_t r = {lhs >= rhs};

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(result_t));

            // Return new stack top
            return resPtr + sizeof(result_t);
        }

        template <typename T>
        OBLO_FORCEINLINE [[nodiscard]] byte* compare_le(byte* stackTop)
        {
            using result_t = u32;

            const T lhs = read_stack<T>(stackTop, 0);
            const T rhs = read_stack<T>(stackTop, sizeof(T));
            const result_t r = {lhs <= rhs};

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(result_t));

            // Return new stack top
            return resPtr + sizeof(result_t);
        }

        template <typename T, typename U>
        OBLO_FORCEINLINE void increment(byte* stackTop, U inc)
        {
            const T base = read_stack<T>(stackTop, 0);
            const T result = base + inc;

            std::memcpy(stackTop - sizeof(T), &result, sizeof(T));
        }

        OBLO_FORCEINLINE void deallocate_stack(byte*& prevTop, byte* newTop)
        {
            if constexpr (g_DebugStackMemory)
            {
                OBLO_ASSERT(prevTop >= newTop);
                std::memset(newTop, 0xcd, prevTop - newTop);
            }

            prevTop = newTop;
        }
    }

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

        if constexpr (g_DebugStackMemory)
        {
            std::memset(m_stackMemory.get(), 0xcd, stackSize);
        }
    }

    void interpreter::load_module(const bytecode_module& m)
    {
        const address_offset baseOffset = m_code.size32();
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
            case bytecode_op::ret:
                return;

            case bytecode_op::push32lo16: {
                u16 v;
                bytecode_payload::unpack_u16(bytecode.payload, v);
                push_u32(v);
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::or32hi16: {
                u16 v;
                bytecode_payload::unpack_u16(bytecode.payload, v);

                byte* const ptr = stack_addr<u32>(m_stackTop, 0);

                u32 val;
                std::memcpy(&val, ptr, sizeof(u32));
                val |= u32{v} << 16;

                std::memcpy(ptr, &val, sizeof(u32));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::pushcppso: {
                u8 size, offset;
                bytecode_payload::unpack_2xu8(bytecode.payload, size, offset);
                const byte* const src = m_stackTop - offset - size;
                byte* const dst = allocate_stack(size);
                std::memcpy(dst, src, size);
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::stru32pso: {
                u8 size, offset;
                bytecode_payload::unpack_2xu8(bytecode.payload, size, offset);
                auto* newTop = m_stackTop - sizeof(u32);
                byte* const dst = newTop - offset - size;
                std::memcpy(dst, newTop, sizeof(u32));
                pop(sizeof(u32));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::retvpso: {
                u8 size, offset;
                bytecode_payload::unpack_2xu8(bytecode.payload, size, offset);

                // Maybe somehow add an assert to check that the range is within the return value part of the stack
                auto* const rvEnd = m_callFrame.back().restoreStackPtr;
                auto* const dst = rvEnd - offset - size;
                auto* const src = m_stackTop - size;

                std::memcpy(dst, src, size);
            }

                ++m_nextInstruction;
                break;

                // Binary add

            case bytecode_op::addu32:
                deallocate_stack(m_stackTop, binary_add<u32>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::addi32:
                deallocate_stack(m_stackTop, binary_add<i32>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::addf32:
                deallocate_stack(m_stackTop, binary_add<f32>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::addu64:
                deallocate_stack(m_stackTop, binary_add<u64>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::addi64:
                deallocate_stack(m_stackTop, binary_add<i64>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::addf64:
                deallocate_stack(m_stackTop, binary_add<f64>(m_stackTop));
                ++m_nextInstruction;
                break;

                // Binary sub

            case bytecode_op::subu32:
                deallocate_stack(m_stackTop, binary_sub<u32>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::subi32:
                deallocate_stack(m_stackTop, binary_sub<i32>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::subf32:
                deallocate_stack(m_stackTop, binary_sub<f32>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::subu64:
                deallocate_stack(m_stackTop, binary_sub<u64>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::subi64:
                deallocate_stack(m_stackTop, binary_sub<i64>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::subf64:
                deallocate_stack(m_stackTop, binary_sub<f64>(m_stackTop));
                ++m_nextInstruction;
                break;

                // Comparison

            case bytecode_op::geu32:
                deallocate_stack(m_stackTop, compare_ge<u32>(m_stackTop));
                ++m_nextInstruction;
                break;

            case bytecode_op::leu32:
                deallocate_stack(m_stackTop, compare_le<u32>(m_stackTop));
                ++m_nextInstruction;
                break;

                // Jump

            case bytecode_op::jmp: {
                const auto addr = read_stack<address_offset>(m_stackTop, 0);
                m_nextInstruction = addr;
                pop(sizeof(address_offset));
            }

            break;

            case bytecode_op::jnz32: {
                const auto addr = read_stack<address_offset>(m_stackTop, 0);
                const auto cond = read_stack<u32>(m_stackTop, sizeof(address_offset));
                pop(sizeof(u32) + sizeof(address_offset));

                m_nextInstruction = cond != 0 ? addr : m_nextInstruction + 1;
            }

            break;

            case bytecode_op::jz32: {
                const auto addr = read_stack<address_offset>(m_stackTop, 0);
                const auto cond = read_stack<u32>(m_stackTop, sizeof(address_offset));
                pop(sizeof(u32) + sizeof(address_offset));

                m_nextInstruction = cond == 0 ? addr : m_nextInstruction + 1;
            }

            break;

                // Increments

            case bytecode_op::incu32pu16: {
                u16 inc;
                bytecode_payload::unpack_u16(bytecode.payload, inc);

                increment<u32>(m_stackTop, inc);
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::inci32pu16: {
                u16 inc;
                bytecode_payload::unpack_u16(bytecode.payload, inc);

                increment<i32>(m_stackTop, inc);
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::incu32poi: {
                u8 offset, inc;
                bytecode_payload::unpack_2xu8(bytecode.payload, offset, inc);

                byte* const var = stack_addr<u32>(m_stackTop, offset);

                u32 content;
                std::memcpy(&content, var, sizeof(u32));

                content += inc;
                std::memcpy(var, &content, sizeof(u32));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::decu32pu16: {
                u16 inc;
                bytecode_payload::unpack_u16(bytecode.payload, inc);

                increment<u32>(m_stackTop, -i32{inc});
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::deci32pu16: {
                u16 inc;
                bytecode_payload::unpack_u16(bytecode.payload, inc);

                increment<i32>(m_stackTop, -i32{inc});
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::instraddr:
                push_u32(m_nextInstruction);
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
        deallocate_stack(m_stackTop, f.restoreStackPtr);

        m_callFrame.pop_back();
    }

    f32 interpreter::read_f32(u32 stackOffset) const
    {
        return read_stack<f32>(m_stackTop, stackOffset);
    }

    u32 interpreter::read_u32(u32 stackOffset) const
    {
        return read_stack<u32>(m_stackTop, stackOffset);
    }

    i32 interpreter::read_i32(u32 stackOffset) const
    {
        return read_stack<i32>(m_stackTop, stackOffset);
    }

    void interpreter::push_u32(u32 value)
    {
        byte* const dst = allocate_stack(sizeof(u32));
        std::memcpy(dst, &value, sizeof(u32));
    }

    void interpreter::pop(u32 stackSize)
    {
        OBLO_ASSERT(m_stackTop - stackSize >= m_stackMemory.get());
        deallocate_stack(m_stackTop, m_stackTop - stackSize);
    }
}