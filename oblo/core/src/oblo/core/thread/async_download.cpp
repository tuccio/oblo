#include <oblo/core/thread/async_download.hpp>

#include <oblo/core/allocator.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/unreachable.hpp>

#include <atomic>

namespace oblo
{
    namespace
    {
        enum class async_download_promise_state : u8
        {
            waiting,
            broken,
            has_value,
        };

        constexpr usize g_DataAlignment = alignof(std::max_align_t);
    }

    struct async_download_promise::control_block
    {
        allocator* allocator;
        std::atomic<u32> refCount;
        std::atomic<async_download_promise_state> state;
        std::span<byte> result;

        void release()
        {
            if (refCount.fetch_sub(1u, std::memory_order_relaxed) == 1)
            {
                allocator->deallocate(result.data(), result.size(), g_DataAlignment);

                this->~control_block();
                allocator->deallocate(reinterpret_cast<byte*>(this), sizeof(control_block), alignof(control_block));
            }
        }
    };

    async_download_promise::async_download_promise() = default;

    async_download_promise::async_download_promise(async_download_promise&& other) noexcept
    {
        m_block = other.m_block;
        other.m_block = nullptr;
    }

    async_download_promise& async_download_promise::operator=(async_download_promise&& other) noexcept
    {
        reset();
        m_block = other.m_block;
        other.m_block = nullptr;
        return *this;
    }

    async_download_promise::~async_download_promise()
    {
        reset();
    }

    void async_download_promise::init(allocator* allocator)
    {
        if (m_block)
        {
            m_block->release();
            m_block = nullptr;
        }

        m_block = new (allocator->allocate(sizeof(control_block), alignof(control_block))) control_block;

        m_block->allocator = allocator;
        m_block->refCount = 1;
        m_block->state = async_download_promise_state::waiting;
    }

    void async_download_promise::set_data(std::span<const byte> bytes)
    {
        populate_data(
            [bytes](allocator* allocator) -> std::span<byte>
            {
                if (!bytes.empty())
                {
                    byte* const ptr = allocator->allocate(bytes.size(), g_DataAlignment);
                    std::memcpy(ptr, bytes.data(), bytes.size());
                    return {ptr, bytes.size()};
                }

                return {};
            });
    }

    void async_download_promise::populate_data(function_ref<std::span<byte>(allocator*)> cb)
    {
        OBLO_ASSERT(m_block);

        if (m_block)
        {
            OBLO_ASSERT(m_block->state == async_download_promise_state::waiting);
            m_block->result = cb(m_block->allocator);

            m_block->state.store(async_download_promise_state::has_value, std::memory_order_release);
        }
    }

    void async_download_promise::reset()
    {
        if (m_block)
        {
            // If somebody is waiting for a result, we are breaking the promise here
            async_download_promise_state e = async_download_promise_state::waiting;
            m_block->state.compare_exchange_strong(e, async_download_promise_state::broken, std::memory_order_relaxed);

            m_block->release();

            m_block = nullptr;
        }
    }

    async_download::async_download() = default;

    async_download::async_download(const async_download_promise& promise) : m_block{promise.m_block}
    {
        if (m_block)
        {
            m_block->refCount.fetch_add(1u);
        }
    }

    async_download::async_download(async_download&& other) noexcept
    {
        m_block = other.m_block;
        other.m_block = nullptr;
    }

    async_download& async_download::operator=(async_download&& other) noexcept
    {
        reset();
        m_block = other.m_block;
        other.m_block = nullptr;
        return *this;
    }

    async_download::~async_download()
    {
        reset();
    }

    expected<std::span<const byte>, async_download::error> async_download::try_get_result() const
    {
        if (!m_block)
        {
            return error::uninitialized;
        }

        const auto state = m_block->state.load(std::memory_order_acquire);

        switch (state)
        {
        case async_download_promise_state::waiting:
            return error::not_ready;

        case async_download_promise_state::broken:
            return error::broken_promise;

        case async_download_promise_state::has_value:
            return m_block->result;

        default:
            unreachable();
        }
    }

    void async_download::reset()
    {
        if (m_block)
        {
            m_block->release();
            m_block = nullptr;
        }
    }
}