#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo
{
    class job_manager;
    struct job_context;

    using job_fn = void (*)(const job_context& ctx);
    using job_userdata_cleanup_fn = void (*)(void* userdata);

    struct job;
    using job_handle = job*;

    template <typename F>
    concept callable_job_ctx = requires(F& f, const job_context& ctx) {
        { f(ctx) };
    };

    template <typename F>
    concept callable_job_no_ctx = requires(F& f) {
        { f() };
    };

    template <typename F>
    concept callable_job = callable_job_ctx<F> || callable_job_no_ctx<F>;

    struct job_manager_config
    {
        THREAD_API static job_manager_config make_default();

        u32 numThreads;
    };

    class job_manager
    {
    public:
        THREAD_API static job_manager* get();

    public:
        THREAD_API job_manager();

        job_manager(const job_manager&) = delete;
        job_manager(job_manager&&) noexcept = delete;

        THREAD_API ~job_manager();

        job_manager& operator=(const job_manager&) = delete;
        job_manager& operator=(job_manager&&) noexcept = delete;

        THREAD_API bool init(const job_manager_config& cfg = job_manager_config::make_default());
        THREAD_API void shutdown();

        /// @brief Creates a waitable child job, that will be destroyed once the execution of its children is completed
        /// and the job is waited for.
        /// @param job The job function.
        /// @param userdata Userdata for the job function call.
        /// @param cleanup Optional cleanup for the userdata.
        /// @return A handle that can be waited on.
        [[nodiscard]] THREAD_API job_handle push_waitable(job_fn job, void* userdata, job_userdata_cleanup_fn cleanup);

        /// @brief Creates a waitable child job, that will be destroyed once the execution of its children is completed
        /// and the job is waited for.
        /// @param parent The parent handle, will be incremented by this call.
        /// @param job The job function.
        /// @param userdata Userdata for the job function call.
        /// @param cleanup Optional cleanup for the userdata.
        /// @return A handle that can be waited on.
        [[nodiscard]] THREAD_API job_handle push_waitable_child(
            job_handle parent, job_fn job, void* userdata, job_userdata_cleanup_fn cleanup);

        /// @brief Creates a non-waitable job, that will be destroyed once the execution of the job itself and its
        /// children is completed.
        /// @param job The job function.
        /// @param userdata Userdata for the job function call.
        /// @param cleanup Optional cleanup for the userdata.
        THREAD_API void push(job_fn job, void* userdata, job_userdata_cleanup_fn cleanup);

        /// @brief Creates a non-waitable job, that will be destroyed once the execution of the job itself and its
        /// children is completed.
        /// @param parent The parent handle, will be incremented by this call.
        /// @param job The job function.
        /// @param userdata Userdata for the job function call.
        /// @param cleanup Optional cleanup for the userdata.
        THREAD_API void push_child(job_handle parent, job_fn job, void* userdata, job_userdata_cleanup_fn cleanup);

        /// @brief Waits for a job to finish, possibly picking up more jobs to complete during the wait.
        /// Jobs with a reference count (i.e. waitable jobs or jobs with manually increased reference) have to be waited
        /// exactly once per reference.
        /// @param job The job to wait for.
        THREAD_API void wait(job_handle job);

        /// @brief Creates a waitable child job, that will be destroyed once the execution of its children is completed
        /// and the job is waited for.
        /// @tparam F A job functor that satisfied callable_job.
        /// @param f The callable job functor, which will be stored for later execution.
        /// @return A handle that can be waited on.
        template <typename F>
            requires callable_job<F>
        [[nodiscard]] job_handle push_waitable(F&& f);

        /// @brief Creates a waitable child job, that will be destroyed once the execution of its children is completed
        /// and the job is waited for.
        /// @tparam F A job functor that satisfied callable_job.
        /// @param parent The parent handle, will be incremented by this call.
        /// @param f The callable job functor, which will be stored for later execution.
        /// @return A handle that can be waited on.
        template <typename F>
            requires callable_job<F>
        [[nodiscard]] job_handle push_waitable_child(job_handle parent, F&& f);

        /// @brief Creates a non-waitable job, that will be destroyed once the execution of the job itself and its
        /// children is completed.
        /// @tparam F A job functor that satisfied callable_job.
        /// @param f The callable job functor, which will be stored for later execution.
        template <typename F>
            requires callable_job<F>
        void push(F&& f);

        /// @brief Creates a non-waitable job, that will be destroyed once the execution of the job itself and its
        /// children is completed.
        /// @tparam F A job functor that satisfied callable_job.
        /// @param parent The parent handle, will be incremented by this call.
        /// @param f The callable job functor, which will be stored for later execution.
        template <typename F>
            requires callable_job<F>
        void push_child(job_handle parent, F&& f);

        /// @brief Explicitly increases the job ref count.
        /// @remarks The function can be used to let multiple threads wait for the same job.
        /// @param job The job to increase the reference of.
        THREAD_API void increase_reference(job_handle job);

        /// @brief Explicitly decreases the job ref count.
        /// @remarks The function can be used to let the system manage the lifetime of a waitable job if waiting is no
        /// longer necessary.
        /// @param job The job to decrease the reference of.
        THREAD_API void decrease_reference(job_handle job);

    private:
        struct impl;

        struct any_callable
        {
            job_fn cb;
            void* userdata;
            job_userdata_cleanup_fn cleanup;
        };

    private:
        static constexpr usize max_soo_size = 32;
        static constexpr usize max_soo_alignment = 16;

    private:
        template <typename F>
        static consteval bool can_inline();

        template <bool IsChild>
        THREAD_API job_handle allocate_job_impl(
            job_handle parent, job_fn job, void* userdata, job_userdata_cleanup_fn cleanup, bool waitable);

        THREAD_API void push_job_impl(job_handle job);

        template <typename F>
            requires callable_job<F>
        any_callable allocate_callable(F&& f);

        template <typename F>
            requires callable_job<F>
        any_callable prepare_inlined_callable();

        [[nodiscard]] THREAD_API void* allocate_userdata(usize size, usize alignment);
        THREAD_API void deallocate_userdata(void* ptr, usize size, usize alignment);

        THREAD_API void* do_inline_buffer(job_handle h);

        template <typename F>
            requires callable_job<F>
        static void job_callback(const job_context& ctx);

    private:
        unique_ptr<impl> m_impl;
    };

    struct job_context
    {
        job_manager* manager;
        job_handle job;
        void* userdata;
        u32 threadId;
    };

    template <typename F>
    consteval bool job_manager::can_inline()
    {
        return sizeof(F) <= max_soo_size && alignof(F) <= max_soo_alignment;
    }

    template <typename F>
        requires callable_job<F>
    job_manager::any_callable job_manager::allocate_callable(F&& f)
    {
        const job_fn cb = &job_callback<F>;

        void* const userdata = allocate_userdata(sizeof(F), alignof(F));
        new (userdata) F{std::forward<F>(f)};

        const job_userdata_cleanup_fn cleanup = [](void* userdata)
        {
            auto* const f = static_cast<F*>(userdata);
            f->~F();

            auto* const jm = get();
            jm->deallocate_userdata(userdata, sizeof(F), alignof(F));
        };

        return {cb, userdata, cleanup};
    }

    template <typename F>
        requires callable_job<F>
    job_manager::any_callable job_manager::prepare_inlined_callable()
    {
        const job_fn cb = &job_callback<F>;

        const job_userdata_cleanup_fn cleanup = [](void* userdata)
        {
            auto* const f = static_cast<F*>(userdata);
            f->~F();
        };

        return {cb, nullptr, cleanup};
    }

    template <typename F>
        requires callable_job<F>
    job_handle job_manager::push_waitable(F&& f)
    {
        any_callable callable;

        if constexpr (can_inline<F>())
        {
            callable = prepare_inlined_callable<F>();
        }
        else
        {
            callable = allocate_callable(std::forward<F>(f));
        }

        const auto h = allocate_job_impl<false>({}, callable.cb, callable.userdata, callable.cleanup, true);

        if constexpr (can_inline<F>())
        {
            new (do_inline_buffer(h)) F{std::forward<F>(f)};
        }

        push_job_impl(h);
        return h;
    }

    template <typename F>
        requires callable_job<F>
    job_handle job_manager::push_waitable_child(job_handle parent, F&& f)
    {
        any_callable callable;

        if constexpr (can_inline<F>())
        {
            callable = prepare_inlined_callable<F>();
        }
        else
        {
            callable = allocate_callable(std::forward<F>(f));
        }

        const auto h = allocate_job_impl<true>(parent, callable.cb, callable.userdata, callable.cleanup, true);

        if constexpr (can_inline<F>())
        {
            new (do_inline_buffer(h)) F{std::forward<F>(f)};
        }

        push_job_impl(h);
        return h;
    }

    template <typename F>
        requires callable_job<F>
    void job_manager::push(F&& f)
    {
        any_callable callable;

        if constexpr (can_inline<F>())
        {
            callable = prepare_inlined_callable<F>();
        }
        else
        {
            callable = allocate_callable(std::forward<F>(f));
        }

        const auto h = allocate_job_impl<false>({}, callable.cb, callable.userdata, callable.cleanup, false);

        if constexpr (can_inline<F>())
        {
            new (do_inline_buffer(h)) F{std::forward<F>(f)};
        }

        push_job_impl(h);
    }

    template <typename F>
        requires callable_job<F>
    void job_manager::push_child(job_handle parent, F&& f)
    {
        any_callable callable;

        if constexpr (can_inline<F>())
        {
            callable = prepare_inlined_callable<F>();
        }
        else
        {
            callable = allocate_callable(std::forward<F>(f));
        }

        const auto h = allocate_job_impl<true>(parent, callable.cb, callable.userdata, callable.cleanup, false);

        if constexpr (can_inline<F>())
        {
            new (do_inline_buffer(h)) F{std::forward<F>(f)};
        }

        push_job_impl(h);
    }

    template <typename F>
        requires callable_job<F>
    void job_manager::job_callback(const job_context& ctx)
    {
        auto& f = *static_cast<F*>(ctx.userdata);

        if constexpr (callable_job_ctx<F>)
        {
            f(ctx);
        }
        else
        {
            f();
        }
    }
}