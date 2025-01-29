#include <coroutine>

namespace oblo::smoke
{
    class [[nodiscard]] test_task
    {
    public:
        struct promise_type;

        using handle = std::coroutine_handle<promise_type>;

    public:
        test_task() = default;
        test_task(const test_task&) = delete;
        test_task(test_task&&) noexcept = delete;

        explicit test_task(const handle& h);

        test_task& operator=(const test_task&) = delete;
        test_task& operator=(test_task&&) noexcept = delete;

        ~test_task();

        void resume() const;

        bool is_done() const;

        bool await_ready() const noexcept;

        void await_suspend(handle h) noexcept;

        void await_resume() const noexcept;

    private:
        handle m_handle{};
    };

    struct test_task::promise_type
    {
        handle nested{};
        handle parent{};

        test_task get_return_object()
        {
            return test_task{handle::from_promise(*this)};
        }

        constexpr std::suspend_never initial_suspend()
        {
            return {};
        }

        constexpr std::suspend_always final_suspend() noexcept
        {
            if (parent)
            {
                parent.promise().nested = {};
            }

            return {};
        }

        constexpr void return_void() {}

        void unhandled_exception() {}
    };

    inline test_task::test_task(const handle& h) : m_handle{h} {}

    inline test_task::~test_task()
    {
        if (m_handle)
        {
            m_handle.destroy();
        }
    }

    inline void test_task::resume() const
    {
        auto& p = m_handle.promise();

        if (p.nested && !p.nested.done())
        {
            p.nested.resume();
        }
        else
        {
            m_handle.resume();
        }
    }

    inline bool test_task::is_done() const
    {
        return m_handle.done();
    }

    inline bool test_task::await_ready() const noexcept
    {
        return false;
    }

    inline void test_task::await_suspend(handle h) noexcept
    {
        h.promise().nested = m_handle;
        m_handle.promise().parent = h;
    }

    inline void test_task::await_resume() const noexcept
    {
        return;
    }
}