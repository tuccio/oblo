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

    private:
        handle m_handle{};
    };

    struct test_task::promise_type
    {
        test_task get_return_object()
        {
            return test_task{handle::from_promise(*this)};
        }

        constexpr std::suspend_always initial_suspend()
        {
            return {};
        }

        constexpr std::suspend_always final_suspend() noexcept
        {
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
        m_handle.resume();
    }

    inline bool test_task::is_done() const
    {
        return m_handle.done();
    }
}