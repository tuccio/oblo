#pragma once

#include <type_traits>
#include <utility>

namespace oblo
{
    template <typename>
    class function_ref;

    template <typename R, typename... Args>
    class function_ref<R(Args...)>
    {
    public:
        function_ref() = default;
        function_ref(const function_ref&) = default;
        function_ref& operator=(const function_ref&) = default;

        template <typename F>
        function_ref(F&& f) : m_userdata{&f}
        {
            m_invoke = [](void* userdata, Args... args)
            { return (*static_cast<std::add_pointer_t<F>>(userdata))(args...); };
        }

        template <typename... T>
        R operator()(T&&... args) const
        {
            return m_invoke(m_userdata, std::forward<T>(args)...);
        }

        explicit operator bool() const noexcept
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