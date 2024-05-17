#include <oblo/smoke/framework.hpp>

namespace oblo::smoke
{
    class do_nothing final : public test
    {
    public:
        test_task run(const test_context&) override
        {
            co_return;
        }
    };

    OBLO_SMOKE_TEST(do_nothing)
}