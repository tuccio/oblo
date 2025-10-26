#include <gtest/gtest.h>

#include <oblo/core/pair.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/thread/future.hpp>

namespace oblo
{
    TEST(future_test, default_uninitialized)
    {
        future<int> f;
        auto result = f.try_get_result();
        ASSERT_FALSE(result.has_value());
        ASSERT_EQ(result.error(), future_error::uninitialized);
    }

    TEST(future_test, set_value)
    {
        promise<int> p;
        p.init();

        future<int> f(p);

        p.set_value(123);

        auto result = f.try_get_result();
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value(), 123);
    }

    TEST(future_test, not_ready_before_set)
    {
        promise<int> p;
        p.init();
        future<int> f(p);

        auto result = f.try_get_result();
        ASSERT_FALSE(result.has_value());
        ASSERT_EQ(result.error(), future_error::not_ready);
    }

    TEST(future_test, broken_promise)
    {
        future<int> f;

        {
            promise<int> p;
            p.init();
            f = future<int>(p);
            // no set_value called; promise goes out of scope
        }

        auto result = f.try_get_result();
        ASSERT_FALSE(result.has_value());
        ASSERT_EQ(result.error(), future_error::broken_promise);
    }

    TEST(future_test, move_construction)
    {
        promise<string> p;
        p.init();

        future<string> f1(p);
        future<string> f2(std::move(f1));

        p.set_value("hello");

        auto result = f2.try_get_result();
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value(), "hello");
    }

    TEST(future_test, reset_clear_state)
    {
        promise<int> p;
        p.init();

        future<int> f(p);

        p.set_value(99);
        const expected result1 = f.try_get_result();
        ASSERT_TRUE(result1.has_value());
        ASSERT_EQ(result1.value(), 99);

        f.reset();
        const expected result2 = f.try_get_result();
        ASSERT_FALSE(result2.has_value());
        ASSERT_EQ(result2.error(), future_error::uninitialized);
    }

    TEST(future_test, move_assignment)
    {
        promise<int> p;
        p.init();

        future<int> f1(p);
        p.set_value(42);

        future<int> f2;
        f2 = std::move(f1);

        const expected result = f2.try_get_result();
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value(), 42);
    }

    TEST(future_test, non_trivial_type)
    {
        promise<pair<int, string>> p;
        p.init();

        future<pair<int, string>> f(p);
        p.set_value(10, "data");

        const expected result = f.try_get_result();
        ASSERT_TRUE(result.has_value());

        auto&& [i, s] = result.value();
        ASSERT_EQ(i, 10);
        ASSERT_EQ(s, "data");
    }
}
