#pragma once

#include <utility>

namespace oblo
{
    template <typename T>
    class final_act : public T
    {
    public:
        template <typename U>
        explicit final_act(U&& u) : T{std::forward<U>(u)}
        {
        }

        ~final_act()
        {
            T::operator()();
        }

        using T::operator();
    };

    template <typename T>
    final_act<T> finally(T&& f)
    {
        return final_act<T>{std::forward<T>(f)};
    }
}