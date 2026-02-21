#include <oblo/gpu/gpu_queue_context.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo::gpu
{
    namespace
    {
        template <typename T, auto Fn>
        void dispose_impl(gpu_instance& gpu, const void* object)
        {
            (gpu.*Fn)(*reinterpret_cast<const T*>(object));
        }
    }

    struct gpu_queue_context::submit_info
    {
        u64 submitIndex{};
        h32<fence> fence{};
    };

    struct gpu_queue_context::disposable_object
    {
        u64 submitIndex;
        void (*cb)(gpu_instance& gpu, const void* object);
        alignas(void*) byte buffer[sizeof(void*)];
    };

    gpu_queue_context::gpu_queue_context() = default;
    gpu_queue_context::~gpu_queue_context() = default;

    result<> gpu_queue_context::init(gpu_instance& instance, h32<queue> queue, u32 maxSubmitsInFlight)
    {
        m_gpu = &instance;
        m_queue = queue;
        m_maxSubmitsInFlight = maxSubmitsInFlight;

        m_submitInfo.assign(maxSubmitsInFlight, {});

        for (auto& info : m_submitInfo)
        {
            const expected f = m_gpu->create_fence({.createSignaled = true});

            if (!f)
            {
                shutdown();
                return f.error();
            }

            info.fence = *f;
        }

        const expected semaphore = m_gpu->create_semaphore({
            .timeline = true,
            .timelineInitialValue = 0u,
        });

        if (!semaphore)
        {
            shutdown();
            return semaphore.error();
        }

        m_timelineSemaphore = *semaphore;

        return no_error;
    }

    void gpu_queue_context::shutdown()
    {
        m_gpu->wait_idle().assert_value();
        destroy_resources_until(m_submitIndex);

        for (auto& info : m_submitInfo)
        {
            if (info.fence)
            {
                m_gpu->destroy_fence(info.fence);
            }
        }

        m_submitInfo.clear();

        if (m_timelineSemaphore)
        {
            m_gpu->destroy_semaphore(m_timelineSemaphore);
            m_timelineSemaphore = {};
        }
    }

    gpu_instance& gpu_queue_context::get_instance() const
    {
        return *m_gpu;
    }

    result<> gpu_queue_context::submit_begin()
    {
        OBLO_PROFILE_SCOPE("gpu_queue_context::submit_begin");

        OBLO_ASSERT(!m_submitInProgress);
        m_submitInProgress = true;

        const u32 submitInfoIdx = m_submitIndex % m_maxSubmitsInFlight;

        auto& submitInfo = m_submitInfo[submitInfoIdx];

        m_lastFinishedSubmit = m_gpu->read_timeline_semaphore(m_timelineSemaphore).value_or(m_lastFinishedSubmit);

        const std::span fences{&submitInfo.fence, 1};

        if (m_lastFinishedSubmit < submitInfo.submitIndex)
        {
            OBLO_PROFILE_SCOPE("wait_for_fences");
            const expected waitResult = m_gpu->wait_for_fences(fences);

            if (!waitResult)
            {
                return waitResult.error();
            }

            m_lastFinishedSubmit = submitInfo.submitIndex;
        }

        destroy_resources_until(m_lastFinishedSubmit);
        return m_gpu->reset_fences(fences);
    }

    result<> oblo::gpu::gpu_queue_context::submit_end(std::span<const hptr<command_buffer>> commandBuffers,
        std::span<const h32<semaphore>> waitSemaphores,
        std::span<const h32<semaphore>> signalSemaphores)
    {
        OBLO_ASSERT(m_submitInProgress);
        const u32 submitInfoIdx = m_submitIndex % m_maxSubmitsInFlight;

        auto& submitInfo = m_submitInfo[submitInfoIdx];

        buffered_array<h32<semaphore>, 8> semaphoresArray;
        semaphoresArray.emplace_back(m_timelineSemaphore);
        semaphoresArray.append(signalSemaphores.begin(), signalSemaphores.end());

        const auto r = m_gpu->submit(m_queue,
            {
                .commandBuffers = commandBuffers,
                .waitSemaphores = waitSemaphores,
                .signalFence = submitInfo.fence,
                .signalSemaphores = semaphoresArray,
            });

        if (r)
        {
            ++m_submitIndex;
            m_submitInProgress = false;
        }

        return r;
    }

    result<> gpu_queue_context::wait_for_submit_completion(u64 submitIndex)
    {
        OBLO_ASSERT(!m_submitInProgress || m_submitIndex > submitIndex);

        if (is_submit_done(submitIndex))
        {
            return no_error;
        }

        const u32 submitInfoIdx = m_submitIndex % m_maxSubmitsInFlight;
        const auto& submitInfo = m_submitInfo[submitInfoIdx];

        const auto r = m_gpu->wait_for_fences({&submitInfo.fence, 1});

        if (!r)
        {
            return r;
        }

        m_lastFinishedSubmit = submitIndex;
        return no_error;
    }

    void gpu_queue_context::destroy_deferred(h32<fence> h, u64 submitIndex)
    {
        auto& o = m_objectsToDispose.emplace_back(submitIndex);
        new (o.buffer) h32<fence>{h};
        o.cb = &dispose_impl<decltype(h), &gpu_instance::destroy_fence>;
    }

    void gpu_queue_context::destroy_deferred(h32<semaphore> h, u64 submitIndex)
    {
        auto& o = m_objectsToDispose.emplace_back(submitIndex);
        new (o.buffer) h32<semaphore>{h};

        o.cb = &dispose_impl<decltype(h), &gpu_instance::destroy_semaphore>;
    }

    void gpu_queue_context::destroy_resources_until(u64 lastCompletedSubmit)
    {
        while (!m_objectsToDispose.empty())
        {
            const auto& o = m_objectsToDispose.front();
            OBLO_ASSERT(o.cb);

            if (o.submitIndex > lastCompletedSubmit)
            {
                break;
            }

            o.cb(*m_gpu, o.buffer);

            m_objectsToDispose.pop_front();
        }
    }
}