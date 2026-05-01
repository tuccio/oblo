#pragma once

#include <atomic>
#include <new>

namespace oblo
{
    class spin_lock
    {
    public:
        spin_lock() = default;
        spin_lock(const spin_lock&) = delete;
        spin_lock(spin_lock&&) noexcept = delete;

        spin_lock& operator=(const spin_lock&) = delete;
        spin_lock& operator=(spin_lock&&) noexcept = delete;

        ~spin_lock() = default;

        void lock() noexcept
        {
            while (m_flag.test_and_set(std::memory_order_acquire))
            {
            }
        }

        void unlock() noexcept
        {
            m_flag.clear(std::memory_order_release);
        }

        bool try_lock() noexcept
        {
            return !m_flag.test_and_set(std::memory_order_acquire);
        }

    private:
        alignas(std::hardware_destructive_interference_size) std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
    };
}