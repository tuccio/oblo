#pragma once

#include <utility>

namespace oblo
{
    template <typename T, typename Function, typename... Ts>
    auto struct_apply(Function&& f, T&& obj)
    {
        if constexpr (requires { [&obj] { auto&& [b1, b2, b3, b4] = obj; }; })
        {
            auto&& [b1, b2, b3, b4] = std::forward<T>(obj);
            return f(b1, b2, b3, b4);
        }
        else if constexpr (requires { [&obj] { auto&& [b1, b2, b3] = obj; }; })
        {
            auto&& [b1, b2, b3] = std::forward<T>(obj);
            return f(b1, b2, b3);
        }
        else if constexpr (requires { [&obj] { auto&& [b1, b2] = obj; }; })
        {
            auto&& [b1, b2] = std::forward<T>(obj);
            return f(b1, b2);
        }
        else if constexpr (requires { [&obj] { auto&& [b1] = obj; }; })
        {
            auto&& [b1] = std::forward<T>(obj);
            f(b1);
        }
        else
        {
            return f();
        }
    }
}