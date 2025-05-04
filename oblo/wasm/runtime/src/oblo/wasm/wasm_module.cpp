#include <oblo/wasm/wasm_module.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/unreachable.hpp>

#include <wasm_export.h>

namespace oblo
{
    namespace
    {
        wasm_module_t as_module(void* p)
        {
            return static_cast<wasm_module_t>(p);
        }

        wasm_module_inst_t as_module_instance(void* p)
        {
            return static_cast<wasm_module_inst_t>(p);
        }

        wasm_exec_env_t as_exec_env(void* p)
        {
            return static_cast<wasm_exec_env_t>(p);
        }

        wasm_function_inst_t as_function(void* p)
        {
            return static_cast<wasm_function_inst_t>(p);
        }

        void convert_args(std::span<const wasm_value> args, dynamic_array<wasm_val_t>& argsArray)
        {
            argsArray.resize_default(args.size());

            for (u32 i = 0; i < argsArray.size32(); ++i)
            {
                auto& v = args[i];
                auto& out = argsArray[i];

                switch (v.type)
                {
                case wasm_type::i32:
                    out = {.kind = WASM_I32, .of = {.i32 = v.value.i32}};
                    break;

                case wasm_type::i64:
                    out = {.kind = WASM_I64, .of = {.i64 = v.value.i64}};
                    break;

                case wasm_type::f32:
                    out = {.kind = WASM_F32, .of = {.f32 = v.value.f32}};
                    break;

                case wasm_type::f64:
                    out = {.kind = WASM_F64, .of = {.f64 = v.value.f64}};
                    break;

                default:
                    unreachable();
                }
            }
        }

        void convert_return_values(const dynamic_array<wasm_val_t>& rvArray, std::span<wasm_value> returnValues)
        {
            for (u32 i = 0; i < rvArray.size32(); ++i)
            {
                auto& v = rvArray[i];
                auto& out = returnValues[i];

                switch (v.kind)
                {
                case WASM_I32:
                    out = {.type = wasm_type::i32, .value = {.i32 = v.of.i32}};
                    break;

                case WASM_I64:
                    out = {.type = wasm_type::i64, .value = {.i64 = v.of.i64}};
                    break;

                case WASM_F32:
                    out = {.type = wasm_type::f32, .value = {.f32 = v.of.f32}};
                    break;

                case WASM_F64:
                    out = {.type = wasm_type::f64, .value = {.f64 = v.of.f64}};
                    break;

                case WASM_V128:
                    OBLO_ASSERT(false);
                    break;

                case WASM_EXTERNREF:
                    OBLO_ASSERT(false);
                    break;
                case WASM_FUNCREF:
                    OBLO_ASSERT(false);
                    break;

                default:
                    unreachable();
                }
            }
        }
    }

    wasm_module::wasm_module(wasm_module&& other) noexcept
    {
        m_module = other.m_module;
        other.m_module = nullptr;
    }

    wasm_module::~wasm_module()
    {
        destroy();
    }

    wasm_module& wasm_module::operator=(wasm_module&& other) noexcept
    {
        destroy();

        m_module = other.m_module;
        other.m_module = nullptr;

        return *this;
    }

    expected<> wasm_module::load(std::span<const byte> wasm, std::span<char> errorBuffer)
    {
        destroy();

        m_module = wasm_runtime_load(reinterpret_cast<u8*>(const_cast<byte*>(wasm.data())),
            u32(wasm.size()),
            errorBuffer.data(),
            u32(errorBuffer.size()));

        if (!m_module)
        {
            return unspecified_error;
        }

        return no_error;
    }

    void wasm_module::destroy()
    {
        if (m_module)
        {
            wasm_runtime_unload(as_module(m_module));
            m_module = nullptr;
        }
    }

    wasm_module_executor::wasm_module_executor(wasm_module_executor&& other) noexcept
    {
        m_instance = other.m_instance;
        m_env = other.m_env;

        other.m_env = nullptr;
        other.m_instance = nullptr;
    }

    wasm_module_executor::~wasm_module_executor()
    {
        destroy();
    }

    wasm_module_executor& wasm_module_executor::operator=(wasm_module_executor&& other) noexcept
    {
        destroy();

        m_instance = other.m_instance;
        m_env = other.m_env;

        other.m_env = nullptr;
        other.m_instance = nullptr;

        return *this;
    }

    expected<> wasm_module_executor::create(
        const wasm_module& module, u32 stackSize, u32 heapSize, std::span<char> errorBuffer)
    {
        destroy();

        const auto instance = wasm_runtime_instantiate(as_module(module.m_module),
            stackSize,
            heapSize,
            errorBuffer.data(),
            u32(errorBuffer.size()));

        if (!instance)
        {
            return unspecified_error;
        }

        const auto env = wasm_runtime_create_exec_env(instance, stackSize);

        if (!env)
        {
            wasm_runtime_deinstantiate(instance);
            return unspecified_error;
        }

        m_instance = instance;
        m_env = env;

        return no_error;
    }

    void wasm_module_executor::destroy()
    {
        if (m_env)
        {
            wasm_runtime_destroy_exec_env(as_exec_env(m_env));
            m_env = nullptr;
        }

        if (m_instance)
        {
            wasm_runtime_deinstantiate(as_module_instance(m_instance));
            m_instance = nullptr;
        }
    }

    wasm_function_ptr wasm_module_executor::find_function(cstring_view name) const
    {
        wasm_function_ptr r;
        r.m_function = wasm_runtime_lookup_function(as_module_instance(m_instance), name.c_str());
        return r;
    }

    expected<> wasm_module_executor::invoke(
        const wasm_function_ptr& function, std::span<wasm_value> returnValues, std::span<const wasm_value> arguments)
    {
        buffered_array<wasm_val_t, 1> rvArray;
        buffered_array<wasm_val_t, 8> argsArray;

        rvArray.resize_default(returnValues.size());
        convert_args(arguments, argsArray);

        if (!wasm_runtime_call_wasm_a(as_exec_env(m_env),
                as_function(function.m_function),
                rvArray.size32(),
                rvArray.data(),
                argsArray.size32(),
                argsArray.data()))
        {
            return unspecified_error;
        }

        convert_return_values(rvArray, returnValues);

        return no_error;
    }

    const char* wasm_module_executor::get_last_exception() const
    {
        return wasm_runtime_get_exception(as_module_instance(m_instance));
    }
}