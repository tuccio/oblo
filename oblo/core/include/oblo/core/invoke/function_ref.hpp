#pragma once

#include <oblo/core/platform/compiler.hpp>

#include <type_traits>
#include <utility>

namespace oblo
{
    template <typename>
    class function_ref;

    template <typename T>
    struct is_function_ref : std::bool_constant<false>
    {
    };

    template <typename T>
    struct is_function_ref<function_ref<T>> : std::bool_constant<true>
    {
    };

    template <typename R, typename... Args>
    class function_ref<R(Args...)>
    {
    public:
        constexpr function_ref() = default;
        constexpr function_ref(const function_ref&) = default;
        constexpr function_ref(function_ref&&) noexcept = default;
        constexpr function_ref& operator=(const function_ref&) = default;
        constexpr function_ref& operator=(function_ref&&) noexcept = default;

        constexpr function_ref(std::nullptr_t) {}

        template <typename F>
            requires(!is_function_ref<std::decay_t<F>>::value)
        constexpr function_ref(F&& f)
        {
            using callable_type = std::remove_reference_t<F>;

            if constexpr (std::is_function_v<callable_type>)
            {
                m_userdata = reinterpret_cast<void*>(+f);
                m_invoke = [](void* userdata, Args... args) -> R
                {
                    auto fn = reinterpret_cast<R (*)(Args...)>(userdata);
                    return fn(std::forward<Args>(args)...);
                };
            }
            else
            {
                m_userdata = std::addressof(f);
                m_invoke = [](void* userdata, Args... args) -> R
                {
                    auto& fn = *static_cast<callable_type*>(userdata);
                    return fn(std::forward<Args>(args)...);
                };
            }
        }

        OBLO_FORCEINLINE constexpr R operator()(Args... args) const
        {
            return m_invoke(m_userdata, std::forward<Args>(args)...);
        }

        OBLO_FORCEINLINE constexpr explicit operator bool() const noexcept
        {
            return m_invoke != nullptr;
        }

        bool operator==(const function_ref& other) const noexcept = default;

    private:
        using invoke_fn = R (*)(void*, Args...);

    private:
        void* m_userdata{};
        invoke_fn m_invoke{};
    };

    template <typename R, typename... Args>
    function_ref(R (*)(Args...)) -> function_ref<R(Args...)>;
}