#pragma once

#include <oblo/core/types.hpp>

#include <deque>

namespace oblo
{
    template <typename T, u32 GenBits>
    class handle_pool
    {
    public:
        T acquire();
        void release(T value);

        static constexpr T get_index(T value);

    private:
        static constexpr T get_gen_mask();
        static constexpr T increment_gen(T value);

    private:
        static constexpr T GenOffset{sizeof(T) * 8 - GenBits};
        static constexpr T GenMask{~T{} << GenOffset};

    private:
        std::deque<T> m_handles;
        T m_lastHandle{};
    };

    template <typename T, u32 GenBits>
    T handle_pool<T, GenBits>::acquire()
    {
        if (m_handles.empty())
        {
            return ++m_lastHandle;
        }

        const auto h = m_handles.front();
        m_handles.pop_front();
        return h;
    }

    template <typename T, u32 GenBits>
    void handle_pool<T, GenBits>::release(T value)
    {
        m_handles.push_back(increment_gen(value));
    }

    template <typename T, u32 GenBits>
    constexpr T handle_pool<T, GenBits>::get_index(T value)
    {
        return value & ~GenMask;
    }

    template <typename T, u32 GenBits>
    constexpr T handle_pool<T, GenBits>::increment_gen(T value)
    {
        static_assert(std::is_unsigned_v<T>);

        const auto gen = (value & GenMask) >> GenOffset;
        const auto newGen = (gen + 1) << GenOffset;

        return newGen | value;
    }
}