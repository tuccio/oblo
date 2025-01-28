#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    enum class handle_pool_policy
    {
        fifo,
        lifo,
    };

    template <typename T, u32 GenBits, handle_pool_policy Policy = handle_pool_policy::fifo>
    class handle_pool
    {
    public:
        handle_pool() = default;
        handle_pool(const handle_pool&) = default;
        handle_pool(handle_pool&&) noexcept = default;
        explicit handle_pool(allocator* allocator) : m_handles{allocator} {}

        handle_pool& operator=(const handle_pool&) = default;
        handle_pool& operator=(handle_pool&&) noexcept = default;

        T acquire();
        void release(T value);

        static constexpr T get_index(T value);

    private:
        static consteval T get_gen_mask()
        {
            if constexpr (GenBits == 0)
            {
                return 0;
            }
            else
            {
                return ~T{} << GenOffset;
            }
        }

        static constexpr T increment_gen(T value);

    private:
        static constexpr T GenOffset{sizeof(T) * 8 - GenBits};
        static constexpr T GenMask{get_gen_mask()};

    private:
        deque<T> m_handles;
        T m_lastHandle{};
    };

    template <typename T, u32 GenBits, handle_pool_policy Policy>
    T handle_pool<T, GenBits, Policy>::acquire()
    {
        if (m_handles.empty())
        {
            return ++m_lastHandle;
        }

        if constexpr (Policy == handle_pool_policy::fifo)
        {
            const auto h = m_handles.front();
            m_handles.pop_front();
            return h;
        }
        else
        {
            const auto h = m_handles.back();
            m_handles.pop_back();
            return h;
        }
    }

    template <typename T, u32 GenBits, handle_pool_policy Policy>
    void handle_pool<T, GenBits, Policy>::release(T value)
    {
        m_handles.push_back(increment_gen(value));
    }

    template <typename T, u32 GenBits, handle_pool_policy Policy>
    constexpr T handle_pool<T, GenBits, Policy>::get_index(T value)
    {
        return value & ~GenMask;
    }

    template <typename T, u32 GenBits, handle_pool_policy Policy>
    constexpr T handle_pool<T, GenBits, Policy>::increment_gen(T value)
    {
        if constexpr (GenBits == 0)
        {
            return value;
        }
        else
        {
            static_assert(std::is_unsigned_v<T>);

            const auto gen = (value & GenMask) >> GenOffset;
            const auto newGen = (gen + 1) << GenOffset;

            return newGen | get_index(value);
        }
    }
}