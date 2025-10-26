#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unreachable.hpp>

#include <atomic>

namespace oblo
{
    enum class future_error : u8
    {
        uninitialized,
        not_ready,
        broken_promise,
    };

    enum class promise_state : u8
    {
        waiting,
        broken,
        has_value,
    };

    template <typename T>
    class future;

    template <typename T>
    class promise
    {
    public:
        promise() = default;
        promise(const promise&) = delete;
        promise(promise&&) noexcept;

        promise& operator=(const promise&) = delete;
        promise& operator=(promise&&) noexcept;

        ~promise();

        void init(allocator* allocator = get_global_allocator());

        template <typename... Args>
        void set_value(Args&&... args);

        bool is_initialized() const;

        void reset();

    private:
        struct control_block
        {
            allocator* allocator;
            std::atomic<u32> refCount;
            std::atomic<promise_state> state;
            alignas(T) byte resultBuffer[sizeof(T)];
            bool isResultConstructed;

            void release();
        };

        friend class future<T>;

    private:
        control_block* m_block = nullptr;
    };

    template <typename T>
    class future
    {
    public:
        using error = future_error;

    public:
        future() = default;
        future(const promise<T>& promise);
        future(const future&) = delete;
        future(future&&) noexcept;

        future& operator=(const future&) = delete;
        future& operator=(future&&) noexcept;

        ~future();

        expected<T&, error> try_get_result() const;

        void reset();

    private:
        promise<T>::control_block* m_block = nullptr;
    };

    template <typename T>
    promise<T>::promise(promise&& other) noexcept
    {
        m_block = other.m_block;
        other.m_block = nullptr;
    }

    template <typename T>
    promise<T>& promise<T>::operator=(promise<T>&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_block = other.m_block;
            other.m_block = nullptr;
        }

        return *this;
    }

    template <typename T>
    promise<T>::~promise()
    {
        reset();
    }

    template <typename T>
    void promise<T>::init(allocator* allocator)
    {
        if (m_block)
        {
            m_block->release();
            m_block = nullptr;
        }

        m_block = new (allocator->allocate(sizeof(control_block), alignof(control_block))) control_block;

        m_block->allocator = allocator;
        m_block->refCount = 1;
        m_block->state = promise_state::waiting;
        m_block->isResultConstructed = false;
    }

    template <typename T>
    template <typename... Args>
    void promise<T>::set_value(Args&&... args)
    {
        OBLO_ASSERT(m_block);

        if (m_block)
        {
            OBLO_ASSERT(m_block->state == promise_state::waiting);
            new (m_block->resultBuffer) T(std::forward<Args>(args)...);
            m_block->state.store(promise_state::has_value, std::memory_order_release);
        }
    }

    template <typename T>
    bool promise<T>::is_initialized() const
    {
        return m_block != nullptr;
    }

    template <typename T>
    void promise<T>::reset()
    {
        if (m_block)
        {
            // If somebody is waiting for a result, we are breaking the promise here
            auto e = promise_state::waiting;
            m_block->state.compare_exchange_strong(e, promise_state::broken, std::memory_order_relaxed);

            m_block->release();

            m_block = nullptr;
        }
    }

    template <typename T>
    void promise<T>::control_block::release()
    {
        if (refCount.fetch_sub(1u, std::memory_order_relaxed) == 1)
        {
            if (isResultConstructed)
            {
                reinterpret_cast<T*>(resultBuffer)->~T();
                isResultConstructed = false;
            }

            auto* const a = allocator;
            this->~control_block();

            a->deallocate(reinterpret_cast<byte*>(this), sizeof(control_block), alignof(control_block));
        }
    }

    template <typename T>
    future<T>::future(const promise<T>& promise) : m_block{promise.m_block}
    {
        if (m_block)
        {
            m_block->refCount.fetch_add(1u);
        }
    }

    template <typename T>
    future<T>::future(future<T>&& other) noexcept
    {
        m_block = other.m_block;
        other.m_block = nullptr;
    }

    template <typename T>
    future<T>& future<T>::operator=(future<T>&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_block = other.m_block;
            other.m_block = nullptr;
        }

        return *this;
    }

    template <typename T>
    future<T>::~future()
    {
        reset();
    }

    template <typename T>
    expected<T&, future_error> future<T>::try_get_result() const
    {
        if (!m_block)
        {
            return error::uninitialized;
        }

        const auto state = m_block->state.load(std::memory_order_acquire);

        switch (state)
        {
        case promise_state::waiting:
            return error::not_ready;

        case promise_state::broken:
            return error::broken_promise;

        case promise_state::has_value:
            return *reinterpret_cast<T*>(m_block->resultBuffer);

        default:
            unreachable();
        }
    }

    template <typename T>
    void future<T>::reset()
    {
        if (m_block)
        {
            m_block->release();
            m_block = nullptr;
        }
    }
}