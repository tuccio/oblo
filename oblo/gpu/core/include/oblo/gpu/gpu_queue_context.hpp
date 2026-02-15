#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/forward.hpp>

#include <span>

namespace oblo::gpu
{
    class gpu_queue_context
    {
    public:
        gpu_queue_context();
        gpu_queue_context(const gpu_queue_context&) = delete;
        gpu_queue_context(gpu_queue_context&&) noexcept = delete;
        ~gpu_queue_context();

        gpu_queue_context& operator=(const gpu_queue_context&) = delete;
        gpu_queue_context& operator=(gpu_queue_context&&) noexcept = delete;

        result<> init(gpu_instance& instance, h32<queue> queue, u32 maxSubmitsInFlight);
        void shutdown();

        gpu_instance& get_instance() const;

        result<> submit_begin();

        result<> submit_end(std::span<const hptr<command_buffer>> commandBuffers,
            std::span<const h32<semaphore>> waitSemaphores,
            std::span<const h32<semaphore>> signalSemaphores);

        u64 get_submit_index() const;
        u64 get_last_finished_submit() const;
        bool is_submit_done(u64 submitIndex) const;

        result<> wait_for_submit_completion(u64 submitIndex);

        void destroy_deferred(h32<buffer> h, u64 submitIndex);
        void destroy_deferred(h32<fence> h, u64 submitIndex);
        void destroy_deferred(h32<image> h, u64 submitIndex);
        void destroy_deferred(h32<semaphore> h, u64 submitIndex);

    private:
        struct submit_info;

    private:
        void destroy_resources_until(u64 lastCompletedSubmit);

    private:
        gpu_instance* m_gpu{};
        dynamic_array<submit_info> m_submitInfo;

        // We want the submit index to start from more than 0, which is the starting value of the semaphore
        u64 m_submitIndex{1};
        u64 m_lastFinishedSubmit{};

        h32<semaphore> m_timelineSemaphore{};
        h32<queue> m_queue{};

        u32 m_maxSubmitsInFlight{};

        bool m_submitInProgress{};
    };

    inline u64 gpu_queue_context::get_submit_index() const
    {
        return m_submitIndex;
    }

    inline u64 gpu_queue_context::get_last_finished_submit() const
    {
        return m_lastFinishedSubmit;
    }

    inline bool gpu_queue_context::is_submit_done(u64 submitIndex) const
    {
        return m_lastFinishedSubmit >= submitIndex;
    }
}