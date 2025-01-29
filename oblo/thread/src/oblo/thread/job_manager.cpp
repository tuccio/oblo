#include <oblo/thread/job_manager.hpp>

#include <oblo/core/compressed_pointer_with_flags.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/trace/profile.hpp>

#include <moodycamel/concurrentqueue.h>

#include <atomic>
#include <format>
#include <memory_resource>
#include <span>
#include <thread>

#define as_job_impl(Job) static_cast<job_impl*>(Job)
#define as_job_handle(Job) static_cast<job_handle>(Job)

namespace oblo
{
    namespace
    {
        constexpr u32 g_hasInlinedUserdata{0};

        struct alignas(64) job_impl
        {
            job_fn function;
            compressed_pointer_with_flags<job_impl, 1> parent;
            alignas(16) void* userdataBuffer[4];
            job_userdata_cleanup_fn cleanup;
            std::atomic<i32> unfinishedJobs;
            std::atomic<i32> references;
        };

        void* get_userdata(job_impl& impl)
        {
            if (impl.parent.get_flag(g_hasInlinedUserdata))
            {
                return impl.userdataBuffer;
            }
            else
            {
                return impl.userdataBuffer[0];
            }
        }
    }

    // This is only really here for debugging purposes, the job is meant to be opaque from the outside
    struct job : job_impl
    {
    };

    namespace
    {
        static_assert(sizeof(job_impl) == 64);
        static_assert(std::is_trivially_destructible_v<job_impl>, "No need to call destructors");

        class job_queue
        {
        public:
            job_queue() = default;
            job_queue(const job_queue&) = delete;
            job_queue(job_queue&&) noexcept = delete;

            job_queue& operator=(const job_queue&) = delete;
            job_queue& operator=(job_queue&&) noexcept = delete;

            void push(job_impl* job)
            {
                m_jobs.enqueue(job);
            }

            bool pop(job_impl*& job)
            {
                return m_jobs.try_dequeue(job);
            }

        private:
            static_assert(moodycamel::ConcurrentQueue<job_impl*>::is_lock_free());
            moodycamel::ConcurrentQueue<job_impl*> m_jobs;
        };

        void* allocate_job()
        {
            return ::operator new(sizeof(job_impl), std::align_val_t(alignof(job_impl)));
        }

        void deallocate_job(job_impl* j)
        {
            ::operator delete(j, std::align_val_t(alignof(job_impl)));
        }

        struct worker_thread_context
        {
            job_manager* manager;
            job_queue* queue;
            u32 id;
        };

        static thread_local constinit worker_thread_context s_tlsWorkerCtx{};

        bool is_worker_thread()
        {
            return s_tlsWorkerCtx.queue != nullptr;
        }

        void increase_reference(job_impl* impl)
        {
            impl->references.fetch_add(1);
        }

        void decrease_reference(job_impl* impl)
        {
            if (impl->references.fetch_sub(1) == 0)
            {
                // No references anymore, we can delete
                deallocate_job(impl);
            }
        }

        void signal_finished_job(job_impl* impl)
        {
            auto* const parent = impl->parent.get_pointer();

            const auto isJobDone = impl->unfinishedJobs.fetch_sub(1) == 1;

            if (isJobDone)
            {
                decrease_reference(impl);
            }

            if (parent)
            {
                signal_finished_job(parent);
            }
        }

        void execute(job_manager* jm, job_impl* impl, u32 threadId)
        {
            void* const userdata = get_userdata(*impl);

            const job_context ctx{
                .manager = jm,
                .job = as_job_handle(impl),
                .userdata = userdata,
                .threadId = threadId,
            };

            impl->function(ctx);

            if (impl->cleanup)
            {
                impl->cleanup(userdata);
            }
        }

        enum class worker_state : u8
        {
            uninitialized,
            ready,
            stop_requested,
        };

        struct worker_thread
        {
            job_queue queue;
            std::jthread thread;
            std::atomic<worker_state> state{worker_state::uninitialized};
        };

        void worker_thread_init(job_manager* manager, u32 id, job_queue* q, std::atomic<worker_state>* state)
        {
#ifdef TRACY_ENABLE
            char threadName[64]{};
            std::format_to_n(threadName, 64, "{}oblo worker #{}", id == 0 ? "main - " : "", id);
            tracy::SetThreadName(threadName);
#endif

            s_tlsWorkerCtx = {
                .manager = manager,
                .queue = q,
                .id = id,
            };

            state->store(worker_state::ready, std::memory_order_release);
        }

        void worker_thread_run(
            job_manager* jm, u32 id, std::span<job_queue* const> queues, const std::atomic<worker_state>* state)
        {
            while (state->load(std::memory_order_relaxed) != worker_state::stop_requested)
            {
                job_impl* job{};

                for (auto* const q : queues)
                {
                    if (q->pop(job))
                    {
                        break;
                    }
                }

                if (!job)
                {
                    std::this_thread::yield();
                }
                else
                {
                    execute(jm, job, id);
                    signal_finished_job(job);
                }
            }
        }
    }

    struct job_manager::impl
    {
        explicit impl(u32 numThreads) : threads{get_global_allocator(), numThreads} {}

        dynamic_array<worker_thread> threads;
        std::pmr::synchronized_pool_resource userdataPool;
    };

    THREAD_API job_manager* job_manager::get()
    {
        return s_tlsWorkerCtx.manager;
    }

    job_manager::job_manager() = default;

    job_manager::~job_manager()
    {
        OBLO_ASSERT(m_impl == nullptr, "This class should be shutdown explicitly.");
    }

    job_handle job_manager::push_waitable(job_fn job, void* userdata, job_userdata_cleanup_fn cleanup)
    {
        const auto h = allocate_job_impl<false>({}, job, userdata, cleanup, true);
        push_job_impl(h);
        return h;
    }

    job_handle job_manager::push_waitable_child(
        job_handle parent, job_fn job, void* userdata, job_userdata_cleanup_fn cleanup)
    {
        const auto h = allocate_job_impl<true>(parent, job, userdata, cleanup, true);
        push_job_impl(h);
        return h;
    }

    void job_manager::push(job_fn job, void* userdata, job_userdata_cleanup_fn cleanup)
    {
        const auto h = allocate_job_impl<false>({}, job, userdata, cleanup, true);
        push_job_impl(h);
    }

    void job_manager::push_child(job_handle parent, job_fn job, void* userdata, job_userdata_cleanup_fn cleanup)
    {
        const auto h = allocate_job_impl<true>(parent, job, userdata, cleanup, true);
        push_job_impl(h);
    }

    template <bool IsChild>
    job_handle job_manager::allocate_job_impl(
        job_handle parent, job_fn f, void* userdata, job_userdata_cleanup_fn cleanup, bool waitable)
    {
        OBLO_ASSERT(is_worker_thread());

        auto* const parentPtr = as_job_impl(parent);

        if constexpr (IsChild)
        {
            for (auto* ancestor = parentPtr; ancestor != nullptr; ancestor = ancestor->parent.get_pointer())
            {
                ancestor->unfinishedJobs.fetch_add(1);
                oblo::increase_reference(ancestor);
            }
        }

        auto* const impl = new (allocate_job()) job_impl{
            .function = f,
            .parent = parentPtr,
            .userdataBuffer = {userdata},
            .cleanup = cleanup,
            .unfinishedJobs = 1,
            .references = i32{waitable},
        };

        return as_job_handle(impl);
    }

    void job_manager::push_job_impl(job_handle h)
    {
        s_tlsWorkerCtx.queue->push(as_job_impl(h));
    }

    bool job_manager::init(const job_manager_config& cfg)
    {
        // The calling thread is also used and will get id 0
        // Other threads are spawned in this fuction
        constexpr u32 mainThreadId = 0;

        if (s_tlsWorkerCtx.manager || cfg.numThreads < 2)
        {
            return false;
        }

        m_impl = allocate_unique<impl>(cfg.numThreads);

        for (u32 i = 1; i < cfg.numThreads; ++i)
        {
            m_impl->threads[i].thread = std::jthread{[this, i]
                {
                    auto& threads = m_impl->threads;
                    auto& thisThread = threads[i];

                    dynamic_array<job_queue*> queues;
                    queues.reserve(threads.size());

                    queues.push_back(&thisThread.queue);

                    for (u32 j = 0; j < threads.size(); ++j)
                    {
                        if (j != i)
                        {
                            queues.push_back(&threads[j].queue);
                        }
                    }

                    worker_thread_init(this, i, &thisThread.queue, &thisThread.state);
                    worker_thread_run(this, i, queues, &thisThread.state);
                }};
        }

        auto& mainThread = m_impl->threads.front();
        worker_thread_init(this, mainThreadId, &mainThread.queue, &mainThread.state);

        for (u32 i = 1; i < cfg.numThreads; ++i)
        {
            auto& t = m_impl->threads[i];

            while (t.state.load(std::memory_order_acquire) == worker_state::uninitialized)
            {
                std::this_thread::yield();
            }
        }

        return true;
    }

    void job_manager::shutdown()
    {
        const auto workers = std::span{m_impl->threads}.subspan(1);

        for (auto& worker : workers)
        {
            worker.state.store(worker_state::stop_requested, std::memory_order_relaxed);
        }

        // TODO: (#51) This actually leaks jobs that are still pending

        for (auto& worker : workers)
        {
            if (worker.thread.joinable())
            {
                worker.thread.join();
            }
        }

        s_tlsWorkerCtx = {};

        m_impl.reset();
    }

    void job_manager::wait(job_handle job)
    {
        if (!job)
        {
            return;
        }

        auto* const impl = as_job_impl(job);

        while (impl->unfinishedJobs.load() > 0)
        {
            job_impl* anyJob;

            if (s_tlsWorkerCtx.queue->pop(anyJob))
            {
                execute(this, anyJob, s_tlsWorkerCtx.id);
                signal_finished_job(anyJob);
            }
        }

        oblo::decrease_reference(impl);
    }

    bool job_manager::try_wait(job_handle job)
    {
        if (!job)
        {
            return false;
        }

        auto* const impl = as_job_impl(job);

        if (impl->unfinishedJobs.load() == 0)
        {
            oblo::decrease_reference(impl);
            return true;
        }

        return false;
    }

    void job_manager::increase_reference(job_handle job)
    {
        oblo::increase_reference(as_job_impl(job));
    }

    void job_manager::decrease_reference(job_handle job)
    {
        oblo::decrease_reference(as_job_impl(job));
    }

    void* job_manager::allocate_userdata(usize size, usize alignment)
    {
        return m_impl->userdataPool.allocate(size, alignment);
    }

    void job_manager::deallocate_userdata(void* ptr, usize size, usize alignment)
    {
        m_impl->userdataPool.deallocate(ptr, size, alignment);
    }

    void* job_manager::do_inline_buffer(job_handle h)
    {
        auto* const impl = as_job_impl(h);
        impl->parent.assign_flag(g_hasInlinedUserdata, true);
        return impl->userdataBuffer;
    }

    THREAD_API job_manager_config job_manager_config::make_default()
    {
        return {
            .numThreads = std::thread::hardware_concurrency(),
        };
    }
}