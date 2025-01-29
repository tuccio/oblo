#pragma once

#include <utility>

namespace oblo
{
    template <typename T>
    class [[nodiscard]] final_act
    {
    public:
        template <typename U>
        explicit final_act(U&& u) : m_finally{std::forward<U>(u)}
        {
        }

        ~final_act()
        {
            m_finally();
        }

    private:
        T m_finally;
    };

    template <typename T>
    final_act<T> finally(T&& f)
    {
        return final_act<T>{std::forward<T>(f)};
    }
}