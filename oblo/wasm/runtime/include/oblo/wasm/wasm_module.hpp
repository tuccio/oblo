#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>

#include <bit>
#include <span>

namespace oblo
{
    enum class wasm_type
    {
        i32,
        i64,
        f32,
        f64,
    };

    struct wasm_value
    {
        wasm_type type;

        union {
            i32 i32;
            i64 i64;
            f32 f32;
            f64 f64;
        } value;
    };

    class wasm_module_executor;
    class wasm_function_ptr;

    class wasm_module
    {
    public:
        wasm_module() = default;
        wasm_module(const wasm_module&) = delete;
        WASM_RT_API wasm_module(wasm_module&&) noexcept;
        WASM_RT_API ~wasm_module();

        wasm_module& operator=(const wasm_module&) = delete;
        WASM_RT_API wasm_module& operator=(wasm_module&&) noexcept;

        WASM_RT_API expected<> load(std::span<const byte> wasm, std::span<char> errorBuffer = {});

        WASM_RT_API void destroy();

    private:
        friend class wasm_module_executor;

    private:
        void* m_module{};
    };

    class wasm_module_executor
    {
    public:
        template <typename T>
        using invoke_result_t = std::conditional_t<std::is_same_v<T, void>, expected<>, expected<T>>;

    public:
        wasm_module_executor() = default;
        wasm_module_executor(const wasm_module_executor&) = delete;
        WASM_RT_API wasm_module_executor(wasm_module_executor&&) noexcept;
        WASM_RT_API ~wasm_module_executor();

        wasm_module_executor& operator=(const wasm_module_executor&) = delete;
        WASM_RT_API wasm_module_executor& operator=(wasm_module_executor&&) noexcept;

        WASM_RT_API expected<> create(
            const wasm_module& module, u32 stackSize, u32 heapSize, std::span<char> errorBuffer = {});

        WASM_RT_API void destroy();

        WASM_RT_API wasm_function_ptr find_function(cstring_view name) const;

        WASM_RT_API expected<> invoke(const wasm_function_ptr& function,
            std::span<wasm_value> returnValues,
            std::span<const wasm_value> arguments);

        template <typename R, typename... Args>
        invoke_result_t<R> invoke(const wasm_function_ptr& function, Args&&... args);

        WASM_RT_API const char* get_last_exception() const;

    private:
        void* m_instance{};
        void* m_env{};
    };

    class wasm_function_ptr
    {
    public:
        explicit operator bool() const noexcept
        {
            return m_function != nullptr;
        }

    private:
        friend class wasm_module;
        friend class wasm_module_executor;

    private:
        void* m_function{};
    };

    template <typename R, typename... Args>
    wasm_module_executor::invoke_result_t<R> wasm_module_executor::invoke(const wasm_function_ptr& function,
        Args&&... args)
    {
        wasm_value rv{};

        constexpr u32 nonEmptyArgsArraySize = sizeof...(Args) == 0 ? 1 : sizeof...(Args);
        wasm_value argsArray[nonEmptyArgsArraySize];

        constexpr auto convertArg = []<typename T>(T&& v) -> wasm_value
        {
            using D = std::decay_t<T>;

            if constexpr (std::is_same_v<i32, D>)
            {
                return {.type = wasm_type::i32, .value = {.i32 = v}};
            }

            if constexpr (std::is_same_v<u32, D>)
            {
                return {.type = wasm_type::i32, .value = {.i32 = std::bit_cast<i32>(v)}};
            }

            if constexpr (std::is_same_v<i64, D>)
            {
                return {.type = wasm_type::i64, .value = {.i64 = v}};
            }

            if constexpr (std::is_same_v<u64, D>)
            {
                return {.type = wasm_type::i64, .value = {.i64 = std::bit_cast<u64>(v)}};
            }

            if constexpr (std::is_same_v<f32, D>)
            {
                return {.type = wasm_type::f32, .value = {.f32 = v}};
            }

            if constexpr (std::is_same_v<f64, D>)
            {
                return {.type = wasm_type::f64, .value = {.f64 = v}};
            }
        };

        u32 i = 0;
        ((argsArray[i++] = convertArg(args)), ...);

        constexpr u32 argumentsCount = std::is_same_v<R, void> ? 0 : 1;

        if (!invoke(function, {&rv, argumentsCount}, {argsArray, sizeof...(Args)}))
        {
            return unspecified_error;
        }

        if constexpr (std::is_same_v<i32, R>)
        {
            return rv.value.i32;
        }

        if constexpr (std::is_same_v<i64, R>)
        {
            return rv.value.i64;
        }

        if constexpr (std::is_same_v<u32, R>)
        {
            return std::bit_cast<u32>(rv.value.i32);
        }

        if constexpr (std::is_same_v<u64, R>)
        {
            return std::bit_cast<u64>(rv.value.i64);
        }

        if constexpr (std::is_same_v<f32, R>)
        {
            return rv.value.f32;
        }

        if constexpr (std::is_same_v<f64, R>)
        {
            return rv.value.f64;
        }

        if constexpr (std::is_same_v<void, R>)
        {
            return no_error;
        }
    }
}