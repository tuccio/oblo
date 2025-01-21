#pragma once

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

        template <typename F>
            requires(!is_function_ref<std::decay_t<F>>::value)
        constexpr function_ref(F&& f) : m_userdata{&f}
        {
            m_invoke = [](void* userdata, Args... args) -> R
            {
                auto&& f = (*static_cast<std::add_pointer_t<F>>(userdata));
                return f(args...);
            };
        }

        template <typename... T>
        OBLO_FORCEINLINE constexpr R operator()(T&&... args) const
        {
            return m_invoke(m_userdata, std::forward<T>(args)...);
        }

        OBLO_FORCEINLINE constexpr explicit operator bool() const noexcept
        {
            return m_invoke != nullptr;
        }

    private:
        using invoke_fn = R (*)(void*, Args...);

    private:
        void* m_userdata{};
        invoke_fn m_invoke{};
    };
}