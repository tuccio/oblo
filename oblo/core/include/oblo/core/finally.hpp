#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/stack_allocator.hpp>

#include <utility>

namespace oblo
{
    template <typename T, bool MayCancel = false>
    class [[nodiscard]] final_act
    {
    public:
        template <typename U>
        explicit final_act(U&& u) : m_finally{std::forward<U>(u)}
        {
        }

        ~final_act()
        {
            if constexpr (MayCancel)
            {
                if (m_cancelled)
                {
                    return;
                }
            }

            m_finally();
        }

        void cancel()
            requires(MayCancel)
        {
            m_cancelled = true;
        }

    private:
        T m_finally;
        bool m_cancelled{};
    };

    template <typename T>
    final_act<T> finally(T&& f)
    {
        return final_act<T>{std::forward<T>(f)};
    }

    template <typename T>
    final_act<T, true> finally_if_not_cancelled(T&& f)
    {
        return final_act<T, true>{std::forward<T>(f)};
    }

    class final_act_queue
    {
    public:
        ~final_act_queue()
        {
            for (auto& c : m_commands)
            {
                c.apply(c.userdata);
            }
        }

        template <typename T>
        void push(T&& f)
        {
            auto r = allocate_storage(std::forward<T>(f));
            m_commands.emplace_back(r, [](void* ud) { (*reinterpret_cast<decltype(r)>(ud))(); });
        }

    private:
        struct command
        {
            using apply_fn = void (*)(void* userdata);

            void* userdata{};
            apply_fn apply{};
        };

        using storage = stack_only_allocator<1u << 12, alignof(std::max_align_t), false>;

    private:
        template <typename T>
        std::add_pointer_t<T> allocate_storage(T&& f)
        {
            static_assert(sizeof(T) < storage::size);

            constexpr usize size = sizeof(T);
            constexpr usize alignment = alignof(T) < storage::alignment ? storage::alignment : alignof(T);

            if (m_storage.empty())
            {
                m_storage.emplace_back();
            }

            auto* ptr = m_storage.back().allocate(size, alignment);

            if (ptr == nullptr)
            {
                m_storage.emplace_back();
                ptr = m_storage.back().allocate(size, alignment);
            }

            OBLO_ASSERT(ptr);

            return new (ptr) T{std::forward<T>(f)};
        }

    private:
        deque<storage> m_storage;
        deque<command> m_commands;
    };
}