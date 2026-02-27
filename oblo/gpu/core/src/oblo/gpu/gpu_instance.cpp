#include <oblo/gpu/gpu_instance.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/trace/profile.hpp>

namespace oblo::gpu
{
    namespace
    {
        constexpr u32 s_MaxTrackedSubmitsInFlight = 16;

        template <typename T>
        bool submit_in_progress(const dynamic_array<T>& submits, u64 submitIndex)
        {
            const u32 i = submitIndex % s_MaxTrackedSubmitsInFlight;
            return submits[i].submitInProgress;
        }

        template <typename T>
        bool submit_in_progress_toggle(dynamic_array<T>& submits, u64 submitIndex)
        {
            const u32 i = submitIndex % s_MaxTrackedSubmitsInFlight;

            auto& submit = submits[i];
            const bool r = submit.submitInProgress;

            submit.submitInProgress = !r;
            return r;
        }
    }

    struct gpu_instance::submit_info
    {
        u64 submitIndex{};
        h32<fence> fence{};
        [[maybe_unused]] bool submitInProgress{};
    };

    struct gpu_instance::disposable_object
    {
        u64 submitIndex;
        void (*cb)(gpu_instance& gpu, const void* object);
        alignas(void*) byte buffer[sizeof(void*)];
    };

    gpu_instance::gpu_instance() = default;
    gpu_instance::~gpu_instance() = default;

    result<> gpu_instance::init_tracked_queue_context()
    {
        m_submitInfo.assign(s_MaxTrackedSubmitsInFlight, {});

        for (auto& info : m_submitInfo)
        {
            const expected f = create_fence({.createSignaled = true});

            if (!f)
            {
                shutdown();
                return f.error();
            }

            info.fence = *f;
        }

        const expected semaphore = create_semaphore({
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

    void gpu_instance::shutdown_tracked_queue_context()
    {
        destroy_tracked_queue_resources_until(m_submitIndex);

        for (auto& info : m_submitInfo)
        {
            if (info.fence)
            {
                destroy(info.fence);
            }
        }

        m_submitInfo.clear();

        if (m_timelineSemaphore)
        {
            destroy(m_timelineSemaphore);
            m_timelineSemaphore = {};
        }
    }

    result<> gpu_instance::begin_tracked_queue_submit()
    {
        OBLO_ASSERT(!submit_in_progress_toggle(m_submitInfo, m_submitIndex));
        OBLO_PROFILE_SCOPE("gpu_instance::begin_tracked_queue_submit");

        const u32 submitInfoIdx = m_submitIndex % s_MaxTrackedSubmitsInFlight;

        auto& submitInfo = m_submitInfo[submitInfoIdx];

        m_lastFinishedSubmit = read_timeline_semaphore(m_timelineSemaphore).assert_value_or(m_lastFinishedSubmit);

        const std::span fences{&submitInfo.fence, 1};

        if (m_lastFinishedSubmit < submitInfo.submitIndex)
        {
            OBLO_PROFILE_SCOPE("wait_for_fences");
            const expected waitResult = wait_for_fences(fences);

            if (!waitResult)
            {
                return waitResult.error();
            }

            m_lastFinishedSubmit = submitInfo.submitIndex;
        }

        destroy_tracked_queue_resources_until(m_lastFinishedSubmit);
        return reset_fences(fences);
    }

    void gpu_instance::end_tracked_queue_submit()
    {
        OBLO_ASSERT(submit_in_progress_toggle(m_submitInfo, m_submitIndex));
        ++m_submitIndex;
    }

    h32<fence> gpu_instance::get_tracked_queue_fence()
    {
        const u32 submitInfoIdx = m_submitIndex % s_MaxTrackedSubmitsInFlight;
        return m_submitInfo[submitInfoIdx].fence;
    }

    result<> gpu_instance::wait_for_submit_completion(u64 submitIndex)
    {
        OBLO_ASSERT(!submit_in_progress(m_submitInfo, m_submitIndex) || m_submitIndex > submitIndex);

        if (is_submit_done(submitIndex))
        {
            return no_error;
        }

        const u32 submitInfoIdx = submitIndex % s_MaxTrackedSubmitsInFlight;
        const auto& submitInfo = m_submitInfo[submitInfoIdx];

        const auto r = wait_for_fences({&submitInfo.fence, 1});

        if (!r)
        {
            return r;
        }

        m_lastFinishedSubmit = submitIndex;
        return no_error;
    }

    template <typename T>
    void gpu_instance::destroy_deferred_impl(h32<T> h, u64 submitIndex)
    {
        auto& o = m_objectsToDispose.emplace_back(submitIndex);
        new (o.buffer) h32<T>{h};
        o.cb = [](gpu_instance& gpu, const void* object) { gpu.destroy(*reinterpret_cast<const h32<T>*>(object)); };
    }

    void gpu_instance::destroy_deferred(h32<acceleration_structure> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<bind_group_layout> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<command_buffer_pool> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<buffer> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<fence> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<graphics_pipeline> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<compute_pipeline> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<raytracing_pipeline> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<image> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<image_pool> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<sampler> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_deferred(h32<semaphore> h, u64 submitIndex)
    {
        destroy_deferred_impl(h, submitIndex);
    }

    void gpu_instance::destroy_tracked_queue_resources_until(u64 lastCompletedSubmit)
    {
        while (!m_objectsToDispose.empty())
        {
            const auto& o = m_objectsToDispose.front();
            OBLO_ASSERT(o.cb);

            if (o.submitIndex > lastCompletedSubmit)
            {
                break;
            }

            o.cb(*this, o.buffer);

            m_objectsToDispose.pop_front();
        }
    }
}