#include <oblo/script/interpreter.hpp>

#include <oblo/core/expected.hpp>
#include <oblo/core/invoke/function_ref.hpp>
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
        OBLO_FORCEINLINE byte* stack_addr_unsafe(byte* stackTop, u32 stackOffset)
        {
            return stackTop - stackOffset - sizeof(T);
        }

        template <typename T>
        OBLO_FORCEINLINE expected<byte*, interpreter_error> stack_addr(
            byte* stackTop, u32 stackOffset, byte* stackMemory)
        {
            byte* const addr = stackTop - stackOffset - sizeof(T);

            if (addr < stackMemory) [[unlikely]]
            {
                return interpreter_error::stack_underflow;
            }

            return addr;
        }

        OBLO_FORCEINLINE expected<byte*, interpreter_error> check_stack_addr(byte* addr, byte* stackMemory)
        {
            if (addr < stackMemory) [[unlikely]]
            {
                return interpreter_error::stack_underflow;
            }

            return addr;
        }

        template <typename T>
        OBLO_FORCEINLINE expected<T, interpreter_error> read_stack(byte* stackTop, u32 stackOffset, byte* stackMemory)
        {
            byte* const addr = stackTop - stackOffset - sizeof(T);

            if (addr < stackMemory) [[unlikely]]
            {
                return interpreter_error::stack_underflow;
            }

            T r;
            std::memcpy(&r, addr, sizeof(T));
            return r;
        }

        template <typename T>
        [[nodiscard]] OBLO_FORCEINLINE expected<byte*, interpreter_error> binary_add(byte* stackTop, byte* stackMemory)
        {
            const expected lhs = read_stack<T>(stackTop, 0, stackMemory);
            const expected rhs = read_stack<T>(stackTop, sizeof(T), stackMemory);

            if (!lhs || !rhs) [[unlikely]]
            {
                return !lhs ? lhs.error() : rhs.error();
            }

            const T r = *lhs + *rhs;

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(T));

            // Return new stack top
            return resPtr + sizeof(T);
        }

        template <typename T>
        [[nodiscard]] OBLO_FORCEINLINE expected<byte*, interpreter_error> binary_sub(byte* stackTop, byte* stackMemory)
        {
            const expected lhs = read_stack<T>(stackTop, 0, stackMemory);
            const expected rhs = read_stack<T>(stackTop, sizeof(T), stackMemory);

            if (!lhs || !rhs) [[unlikely]]
            {
                return !lhs ? lhs.error() : rhs.error();
            }

            const T r = *lhs - *rhs;

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(T));

            // Return new stack top
            return resPtr + sizeof(T);
        }

        template <typename T>
        [[nodiscard]] OBLO_FORCEINLINE expected<byte*, interpreter_error> binary_mul(byte* stackTop, byte* stackMemory)
        {
            const expected lhs = read_stack<T>(stackTop, 0, stackMemory);
            const expected rhs = read_stack<T>(stackTop, sizeof(T), stackMemory);

            if (!lhs || !rhs) [[unlikely]]
            {
                return !lhs ? lhs.error() : rhs.error();
            }

            const T r = *lhs * *rhs;

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(T));

            // Return new stack top
            return resPtr + sizeof(T);
        }

        template <typename T>
        [[nodiscard]] OBLO_FORCEINLINE expected<byte*, interpreter_error> binary_vec_mul(
            byte* stackTop, byte* stackMemory, u32 count)
        {
            const u32 vecByteSize = u32(sizeof(T) * count);

            // Consume 2 args but keep space for the result
            auto* const resPtr = stackTop - 2 * vecByteSize;

            for (u32 i = 0; i < count; ++i)
            {
                const u32 currentOffset = u32(i * sizeof(T));

                const expected lhs = read_stack<T>(stackTop, currentOffset, stackMemory);
                const expected rhs = read_stack<T>(stackTop, currentOffset + vecByteSize, stackMemory);

                if (!lhs || !rhs) [[unlikely]]
                {
                    return !lhs ? lhs.error() : rhs.error();
                }

                const T r = *lhs * *rhs;

                std::memcpy(resPtr + currentOffset, &r, sizeof(T));
            }

            // Return new stack top
            return resPtr + sizeof(T) * count;
        }

        template <typename T>
        [[nodiscard]] OBLO_FORCEINLINE expected<byte*, interpreter_error> binary_div(byte* stackTop, byte* stackMemory)
        {
            const expected lhs = read_stack<T>(stackTop, 0, stackMemory);
            const expected rhs = read_stack<T>(stackTop, sizeof(T), stackMemory);

            if (!lhs || !rhs) [[unlikely]]
            {
                return !lhs ? lhs.error() : rhs.error();
            }

            const T r = *lhs / *rhs;

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(T));

            // Return new stack top
            return resPtr + sizeof(T);
        }

        template <typename T>
        [[nodiscard]] OBLO_FORCEINLINE expected<byte*, interpreter_error> compare_ge(byte* stackTop, byte* stackMemory)
        {
            using result_t = u32;

            const expected lhs = read_stack<T>(stackTop, 0, stackMemory);
            const expected rhs = read_stack<T>(stackTop, sizeof(T), stackMemory);

            if (!lhs || !rhs) [[unlikely]]
            {
                return !lhs ? lhs.error() : rhs.error();
            }

            const result_t r = {*lhs >= *rhs};

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(result_t));

            // Return new stack top
            return resPtr + sizeof(result_t);
        }

        template <typename T>
        [[nodiscard]] OBLO_FORCEINLINE expected<byte*, interpreter_error> compare_le(byte* stackTop, byte* stackMemory)
        {
            using result_t = u32;

            const expected lhs = read_stack<T>(stackTop, 0, stackMemory);
            const expected rhs = read_stack<T>(stackTop, sizeof(T), stackMemory);

            if (!lhs || !rhs) [[unlikely]]
            {
                return !lhs ? lhs.error() : rhs.error();
            }

            const result_t r = {*lhs <= *rhs};

            // Consume 2 args but keep space for the result
            auto* resPtr = stackTop - 2 * sizeof(T);
            std::memcpy(resPtr, &r, sizeof(result_t));

            // Return new stack top
            return resPtr + sizeof(result_t);
        }

        template <typename T, typename U>
        OBLO_FORCEINLINE expected<void, interpreter_error> increment(byte* stackTop, U inc, byte* stackMemory)
        {
            const expected ptr = stack_addr<T>(stackTop, 0, stackMemory);

            if (!ptr) [[unlikely]]
            {
                return ptr.error();
            }

            T base;
            std::memcpy(&base, ptr.value(), sizeof(T));
            const T result = base + inc;

            std::memcpy(stackTop - sizeof(T), &result, sizeof(T));
            return no_error;
        }

        OBLO_FORCEINLINE void deallocate_stack_unsafe(byte*& prevTop, byte* newTop)
        {
            if constexpr (g_DebugStackMemory)
            {
                OBLO_ASSERT(prevTop >= newTop);
                std::memset(newTop, 0xcd, prevTop - newTop);
            }

            prevTop = newTop;
        }

        OBLO_FORCEINLINE expected<void, interpreter_error> deallocate_stack(
            byte*& prevTop, byte* newTop, byte* stackMemory)
        {
            if (newTop < stackMemory) [[unlikely]]
            {
                return interpreter_error::stack_underflow;
            }

            deallocate_stack_unsafe(prevTop, newTop);
            return no_error;
        }

        struct script_string_ref
        {
            u32 isReadOnlyString : 1;
            u32 stackStringLength : 31;
            u32 stackAddressOrReadOnlyId;
        };

        struct script_data_ref
        {
            u32 dataLength;
            u32 stackAddress;
        };
    }

    static_assert(script_string_ref_size() == sizeof(script_string_ref));
    static_assert(script_data_ref_size() == sizeof(script_data_ref));

    struct interpreter::call_frame
    {
        instruction_idx returnAddr;
        u32 returnSize;
        byte* restoreStackPtr;
        byte returnBuffer[16];
    };

    struct interpreter::function_info
    {
        u32 paramsSize;
        u32 returnSize;
        instruction_idx address;
    };

    struct interpreter::read_only_string
    {
        string data;
        usize hash;
    };

    struct interpreter::runtime_tag
    {
        script_data_ref dataRef;
    };

    struct interpreter::native_function
    {
        script_api_fn fn;
        u32 paramsSize;
        u32 returnSize;
    };

    interpreter::interpreter() = default;

    interpreter::~interpreter() = default;

    void interpreter::init(u32 stackSize)
    {
        m_stackMemory = allocate_unique<byte[]>(stackSize);
        m_stackTop = m_stackMemory.get();
        m_stackMax = m_stackTop + stackSize;
        m_nextInstruction = 0;
        m_code.clear();
        m_callFrame.clear();
        m_readOnlyStrings.clear();
        m_runtimeTags.clear();
        m_apiFunctions.clear();

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

        m_readOnlyStrings.clear();
        m_readOnlyStrings.reserve(m.readOnlyStrings.size());

        for (const auto& str : m.readOnlyStrings)
        {
            m_readOnlyStrings.push_back({
                .data = str,
                .hash = hash<string>{}(str),
            });
        }
    }

    void interpreter::register_api(string_view name, script_api_fn fn, u32 paramsSize, u32 returnSize)
    {
        m_apiFunctions[string{name}] = {
            .fn = fn,
            .paramsSize = paramsSize,
            .returnSize = returnSize,
        };
    }

    h32<script_function> interpreter::find_function(hashed_string_view name) const
    {
        const auto it = m_functionNames.find(name);
        return it == m_functionNames.end() ? h32<script_function>{} : h32<script_function>{it->second};
    }

    expected<void, interpreter_error> interpreter::call_function(h32<script_function> f)
    {
        const auto fnInfo = m_functions[f.value];

        if (const auto e = begin_call(fnInfo.paramsSize, fnInfo.returnSize, m_nextInstruction); !e)
        {
            return e.error();
        }

        m_nextInstruction = fnInfo.address;

        const auto e = run();
        finish_call();

        return e;
    }

    expected<void, interpreter_error> interpreter::set_function_return(std::span<const byte> data)
    {
        auto& callFrame = m_callFrame.back();

        if (data.size() != callFrame.returnSize)
        {
            return interpreter_error::invalid_arguments;
        }

        std::memcpy(callFrame.returnBuffer, data.data(), data.size());
        return no_error;
    }

    u32 interpreter::used_stack_size() const
    {
        return u32(m_stackTop - m_stackMemory.get());
    }

    u32 interpreter::available_stack_size() const
    {
        return u32(m_stackMax - m_stackTop);
    }

    expected<void, interpreter_error> interpreter::run()
    {
        OBLO_ASSERT(m_nextInstruction < m_code.size());

#define OBLO_INTERPRETER_ABORT(err)                                                                                    \
    {                                                                                                                  \
        return err;                                                                                                    \
    }

#define OBLO_INTERPRETER_ABORT_ON_ERROR(e)                                                                             \
    if (!e) [[unlikely]]                                                                                               \
    {                                                                                                                  \
        OBLO_INTERPRETER_ABORT(e.error());                                                                             \
    }

        while (true)
        {
            const bytecode_instruction bytecode = m_code[m_nextInstruction];

            switch (bytecode.op)
            {
            case bytecode_op::ret:
                if (!m_callFrame.empty())
                {
                    const auto returnSize = m_callFrame.back().returnSize;
                    byte* const returnValue = m_stackTop - returnSize;

                    if (const auto e = check_stack_addr(returnValue, m_stackMemory.get()); !e)
                    {
                        return e.error();
                    }

                    return set_function_return({returnValue, returnSize});
                }

                return no_error;

            case bytecode_op::push_32lo16: {
                u16 v;
                bytecode_payload::unpack_u16(bytecode.payload, v);
                push_u32(v);
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::or_32hi16: {
                u16 v;
                bytecode_payload::unpack_u16(bytecode.payload, v);

                const expected ptr = stack_addr<u32>(m_stackTop, 0, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(ptr);

                u32 val;
                std::memcpy(&val, *ptr, sizeof(u32));
                val |= u32{v} << 16;

                std::memcpy(*ptr, &val, sizeof(u32));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::push_copy_sizeoffset: {
                u8 size, offset;
                bytecode_payload::unpack_2xu8(bytecode.payload, size, offset);

                const expected srcAddr = check_stack_addr(m_stackTop - offset - size, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(srcAddr);

                byte* const dst = allocate_stack(size);
                std::memcpy(dst, *srcAddr, size);
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::push_read_only_string_view: {
                u16 stringId;
                bytecode_payload::unpack_u16(bytecode.payload, stringId);

                const script_string_ref strRef{
                    .isReadOnlyString = true,
                    .stackAddressOrReadOnlyId = stringId,
                };

                OBLO_INTERPRETER_ABORT_ON_ERROR(push_data(as_bytes(std::span{&strRef, 1})));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::store_32_sizeoffset: {
                u8 size, offset;
                bytecode_payload::unpack_2xu8(bytecode.payload, size, offset);
                const expected dstAddr = stack_addr<u32>(m_stackTop, offset, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(dstAddr);

                const expected srcAddr = stack_addr<u32>(m_stackTop, 0, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(srcAddr);

                std::memcpy(*dstAddr, *srcAddr, sizeof(u32));
                deallocate_stack_unsafe(m_stackTop, *srcAddr);
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::push_value_sizeoffset: {
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

            case bytecode_op::add_u32: {
                const expected v = binary_add<u32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::add_i32: {
                const expected v = binary_add<i32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::add_f32: {
                const expected v = binary_add<f32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::add_u64: {
                const expected v = binary_add<u64>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::add_i64: {
                const expected v = binary_add<i64>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::add_f64: {
                const expected v = binary_add<f64>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

                // Binary sub

            case bytecode_op::sub_u32: {
                const expected v = binary_sub<u32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::sub_i32: {
                const expected v = binary_sub<i32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::sub_f32: {
                const expected v = binary_sub<f32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::sub_u64: {
                const expected v = binary_sub<u64>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::sub_i64: {
                const expected v = binary_sub<i64>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::sub_f64: {
                const expected v = binary_sub<f64>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

                // Binary div
            case bytecode_op::div_f32: {
                const expected v = binary_mul<f32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

                // Binary vec mul
            case bytecode_op::mul_vec_f32: {
                u16 components;
                bytecode_payload::unpack_u16(bytecode.payload, components);

                const expected v = binary_vec_mul<f32>(m_stackTop, m_stackMemory.get(), components);
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

                // Comparison

            case bytecode_op::ge_u32: {
                const expected v = compare_ge<u32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::le_u32: {
                const expected v = compare_le<u32>(m_stackTop, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(v);
                deallocate_stack_unsafe(m_stackTop, *v);
            }
                ++m_nextInstruction;
                break;

                // Jump

            case bytecode_op::jmp: {
                const expected addr = read_stack<address_offset>(m_stackTop, 0, m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(addr);
                m_nextInstruction = *addr;
                deallocate_stack_unsafe(m_stackTop, m_stackTop - sizeof(address_offset));
            }
            break;

            case bytecode_op::jnz32: {
                const expected addr = read_stack<address_offset>(m_stackTop, 0, m_stackMemory.get());
                const expected cond = read_stack<u32>(m_stackTop, sizeof(address_offset), m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(addr);
                OBLO_INTERPRETER_ABORT_ON_ERROR(cond);
                deallocate_stack_unsafe(m_stackTop, m_stackTop - (sizeof(u32) + sizeof(address_offset)));

                m_nextInstruction = *cond != 0 ? *addr : m_nextInstruction + 1;
            }

            break;

            case bytecode_op::jz32: {
                const expected addr = read_stack<address_offset>(m_stackTop, 0, m_stackMemory.get());
                const expected cond = read_stack<u32>(m_stackTop, sizeof(address_offset), m_stackMemory.get());
                OBLO_INTERPRETER_ABORT_ON_ERROR(addr);
                OBLO_INTERPRETER_ABORT_ON_ERROR(cond);
                deallocate_stack_unsafe(m_stackTop, m_stackTop - (sizeof(u32) + sizeof(address_offset)));

                m_nextInstruction = *cond == 0 ? *addr : m_nextInstruction + 1;
            }

            break;

                // Increments

            case bytecode_op::increment_u32_val: {
                u16 inc;
                bytecode_payload::unpack_u16(bytecode.payload, inc);

                OBLO_INTERPRETER_ABORT_ON_ERROR(increment<u32>(m_stackTop, inc, m_stackMemory.get()));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::increment_i32_val: {
                u16 inc;
                bytecode_payload::unpack_u16(bytecode.payload, inc);

                OBLO_INTERPRETER_ABORT_ON_ERROR(increment<i32>(m_stackTop, inc, m_stackMemory.get()));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::increment_stackref_u32_offsetval: {
                u8 offset, inc;
                bytecode_payload::unpack_2xu8(bytecode.payload, offset, inc);

                const expected varAddr = stack_addr<u32>(m_stackTop, offset, m_stackMemory.get());

                u32 content;
                std::memcpy(&content, *varAddr, sizeof(u32));

                content += inc;
                std::memcpy(*varAddr, &content, sizeof(u32));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::decrement_u32_u16: {
                u16 inc;
                bytecode_payload::unpack_u16(bytecode.payload, inc);

                OBLO_INTERPRETER_ABORT_ON_ERROR(increment<u32>(m_stackTop, -i32{inc}, m_stackMemory.get()));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::decrement_i32_val: {
                u16 inc;
                bytecode_payload::unpack_u16(bytecode.payload, inc);

                OBLO_INTERPRETER_ABORT_ON_ERROR(increment<i32>(m_stackTop, -i32{inc}, m_stackMemory.get()));
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::push_current_instruction_address:
                push_u32(m_nextInstruction);
                ++m_nextInstruction;
                break;

            case bytecode_op::tag_data_ref_static: {
                u16 stringId;
                bytecode_payload::unpack_u16(bytecode.payload, stringId);

                if (stringId >= m_readOnlyStrings.size()) [[unlikely]]
                {
                    OBLO_INTERPRETER_ABORT(interpreter_error::invalid_string);
                }

                if (stringId >= m_runtimeTags.size())
                {
                    m_runtimeTags.resize_default(stringId + 1);
                }

                const expected dataRef = read_stack<script_data_ref>(m_stackTop, 0, m_stackMemory.get());

                if (!dataRef)
                {
                    return dataRef.error();
                }

                m_runtimeTags[stringId] = {
                    .dataRef = *dataRef,
                };
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::push_stack_top_ref: {
                u16 size;
                bytecode_payload::unpack_u16(bytecode.payload, size);

                const auto address = m_stackTop - m_stackMemory.get() - size;

                if (address < 0)
                {
                    return interpreter_error::stack_underflow;
                }

                const expected e = push_data_view(stack_address(address), size);

                if (!e)
                {
                    return e.error();
                }
            }

                ++m_nextInstruction;
                break;

            case bytecode_op::push_tagged_data_ref_static: {
                u16 stringId;
                bytecode_payload::unpack_u16(bytecode.payload, stringId);

                if (stringId >= m_runtimeTags.size()) [[unlikely]]
                {
                    OBLO_INTERPRETER_ABORT(interpreter_error::invalid_tag);
                }

                const auto& dataRef = m_runtimeTags[stringId].dataRef;
                const expected e = push_data_view(dataRef.stackAddress, dataRef.dataLength);

                if (!e)
                {
                    return e.error();
                }
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::push_tagged_data_copy_static: {
                u16 stringId;
                bytecode_payload::unpack_u16(bytecode.payload, stringId);

                if (stringId >= m_runtimeTags.size()) [[unlikely]]
                {
                    OBLO_INTERPRETER_ABORT(interpreter_error::invalid_tag);
                }

                const auto& dataRef = m_runtimeTags[stringId].dataRef;

                const expected e = push_data(std::span(m_stackMemory.get() + dataRef.stackAddress, dataRef.dataLength));

                if (!e)
                {
                    return e.error();
                }
            }
                ++m_nextInstruction;
                break;

            case bytecode_op::call_api_static: {
                u16 stringId;
                bytecode_payload::unpack_u16(bytecode.payload, stringId);

                if (stringId >= m_readOnlyStrings.size()) [[unlikely]]
                {
                    OBLO_INTERPRETER_ABORT(interpreter_error::invalid_string);
                }

                const auto& readOnlyStr = m_readOnlyStrings[stringId];
                const auto it = m_apiFunctions.find(hashed_string_view{readOnlyStr.data, readOnlyStr.hash});

                if (it != m_apiFunctions.end())
                {
                    const native_function nativeFunc = it->second;

                    OBLO_INTERPRETER_ABORT_ON_ERROR(
                        begin_call(nativeFunc.paramsSize, nativeFunc.returnSize, m_nextInstruction + 1));

                    OBLO_INTERPRETER_ABORT_ON_ERROR(nativeFunc.fn(*this));

                    finish_call();
                }
                else
                {
                    OBLO_INTERPRETER_ABORT(interpreter_error::unknown_function);
                }
            }

            break;

            case bytecode_op::cos_vec_f32:
                [[fallthrough]];

            case bytecode_op::sin_vec_f32:
                [[fallthrough]];

            case bytecode_op::tan_vec_f32:
                [[fallthrough]];

            case bytecode_op::atan_vec_f32: {
                using op_t = float (*)(float);

                op_t op{};

                switch (bytecode.op)
                {
                case bytecode_op::cos_vec_f32:
                    op = op_t{&std::cos};
                    break;

                case bytecode_op::sin_vec_f32:
                    op = op_t{&std::sin};
                    break;

                case bytecode_op::tan_vec_f32:
                    op = op_t{&std::tan};
                    break;

                case bytecode_op::atan_vec_f32:
                    op = op_t{&std::atan};
                    break;

                default:
                    unreachable();
                }

                u16 count;
                bytecode_payload::unpack_u16(bytecode.payload, count);

                const u32 vecByteSize = u32(sizeof(f32) * count);

                // Consume 2 args but keep space for the result
                auto* const resPtr = m_stackTop - vecByteSize;

                for (u32 i = 0; i < count; ++i)
                {
                    const u32 currentOffset = u32(i * sizeof(f32));

                    const expected arg = read_stack<f32>(m_stackTop, currentOffset, m_stackMemory.get());

                    if (!arg) [[unlikely]]
                    {
                        return arg.error();
                    }

                    const f32 r = op(*arg);

                    std::memcpy(resPtr + currentOffset, &r, sizeof(f32));
                }

                // Consume the arg
                OBLO_ASSERT(m_stackTop == resPtr + vecByteSize);
            }

                ++m_nextInstruction;
                break;

            default:
                OBLO_ASSERT(false);
                return interpreter_error::unknown_instruction;
            }
        }

        return no_error;
    }

    void interpreter::reset_execution()
    {
        m_nextInstruction = 0;
        deallocate_stack_unsafe(m_stackTop, m_stackMemory.get());

        m_callFrame.clear();
    }

    byte* interpreter::allocate_stack(u32 size)
    {
        byte* const r = m_stackTop;
        m_stackTop += size;

        OBLO_ASSERT(m_stackTop <= m_stackMax);
        return r;
    }

    expected<void, interpreter_error> interpreter::begin_call(
        u32 paramsSize, u32 returnSize, instruction_idx returnAddr)
    {
        if (used_stack_size() < paramsSize)
        {
            return interpreter_error::invalid_arguments;
        }

        if (returnSize > sizeof(call_frame::returnBuffer))
        {
            return interpreter_error::stack_overflow;
        }

        auto& callFrame = m_callFrame.push_back_default();
        callFrame.returnSize = returnSize;
        callFrame.returnAddr = returnAddr;
        callFrame.restoreStackPtr = m_stackTop - paramsSize + returnSize;

        return no_error;
    }

    void interpreter::finish_call()
    {
        const auto& f = m_callFrame.back();

        m_nextInstruction = f.returnAddr;
        m_stackTop = f.restoreStackPtr;

        if (f.returnSize > 0)
        {
            std::memcpy(f.restoreStackPtr - f.returnSize, f.returnBuffer, f.returnSize);
        }

        m_callFrame.pop_back();
    }

    expected<f32, interpreter_error> interpreter::read_f32(u32 stackOffset) const
    {
        return read_stack<f32>(m_stackTop, stackOffset, m_stackMemory.get());
    }

    expected<u32, interpreter_error> interpreter::read_u32(u32 stackOffset) const
    {
        return read_stack<u32>(m_stackTop, stackOffset, m_stackMemory.get());
    }

    expected<i32, interpreter_error> interpreter::read_i32(u32 stackOffset) const
    {
        return read_stack<i32>(m_stackTop, stackOffset, m_stackMemory.get());
    }

    expected<string_view, interpreter_error> interpreter::get_string_view(u32 stackOffset) const
    {
        const expected stringRef = read_stack<script_string_ref>(m_stackTop, stackOffset, m_stackMemory.get());

        if (!stringRef)
        {
            return stringRef.error();
        }

        if (stringRef->isReadOnlyString)
        {
            return m_readOnlyStrings[stringRef->stackAddressOrReadOnlyId].data;
        }
        else
        {
            const auto address = stringRef->stackAddressOrReadOnlyId;
            const auto length = stringRef->stackStringLength;

            const byte* stringBegin = m_stackMemory.get() + address;
            const byte* stringEnd = stringBegin + length;

            if (stringEnd > m_stackMemory.get()) [[unlikely]]
            {
                return interpreter_error::stack_overflow;
            }

            return string_view{reinterpret_cast<const char*>(stringBegin), reinterpret_cast<const char*>(stringEnd)};
        }
    }

    expected<std::span<const byte>, interpreter_error> interpreter::get_data_view(u32 stackOffset) const
    {
        const expected dataRef = read_stack<script_data_ref>(m_stackTop, stackOffset, m_stackMemory.get());

        if (!dataRef)
        {
            return dataRef.error();
        }

        const auto address = dataRef->stackAddress;
        const auto length = dataRef->dataLength;

        const byte* dataBegin = m_stackMemory.get() + address;
        const byte* dataEnd = dataBegin + length;

        if (dataEnd > m_stackTop) [[unlikely]]
        {
            return interpreter_error::stack_overflow;
        }

        return std::span{dataBegin, dataEnd};
    }

    void interpreter::push_u32(u32 value)
    {
        byte* const dst = allocate_stack(sizeof(u32));
        std::memcpy(dst, &value, sizeof(u32));
    }

    expected<interpreter::stack_address, interpreter_error> interpreter::push_data(std::span<const byte> data)
    {
        const u32 bytesCount = u32(data.size());
        byte* const dst = allocate_stack(bytesCount);
        std::memcpy(dst, data.data(), bytesCount);
        return narrow_cast<u32>(m_stackTop - m_stackMemory.get());
    }

    expected<void, interpreter_error> interpreter::push_data_view(stack_address address, u32 size)
    {
        const script_data_ref dataRef{
            .dataLength = size,
            .stackAddress = address,
        };

        const auto e = push_data(as_bytes(std::span{&dataRef, 1}));

        if (!e)
        {
            return e.error();
        }

        return no_error;
    }

    expected<void, interpreter_error> interpreter::push_string_view(stack_address address, u32 size)
    {
        const script_string_ref strRef{
            .isReadOnlyString = false,
            .stackStringLength = size,
            .stackAddressOrReadOnlyId = address,
        };

        const auto e = push_data(as_bytes(std::span{&strRef, 1}));

        if (!e)
        {
            return e.error();
        }

        return no_error;
    }

    expected<void, interpreter_error> interpreter::pop(u32 stackSize)
    {
        return deallocate_stack(m_stackTop, m_stackTop - stackSize, m_stackMemory.get());
    }
}