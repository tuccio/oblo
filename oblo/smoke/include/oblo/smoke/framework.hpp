#pragma once

#include <oblo/core/finally.hpp>
#include <oblo/smoke/framework/test.hpp>
#include <oblo/smoke/framework/test_context.hpp>
#include <oblo/smoke/framework/test_fixture.hpp>
#include <oblo/smoke/framework/test_task.hpp>

namespace testing
{
    class Test;
}

inline thread_local testing::Test* g_currentTest{};

#define OBLO_SMOKE_TEST(Class)                                                                                         \
    TEST(smoke_test, Class)                                                                                            \
    {                                                                                                                  \
        g_currentTest = this;                                                                                          \
        const auto cleanup = finally([] { g_currentTest = nullptr; });                                                 \
                                                                                                                       \
        Class test;                                                                                                    \
        ::oblo::smoke::test_fixture fixture;                                                                           \
                                                                                                                       \
        if (HasFatalFailure())                                                                                         \
        {                                                                                                              \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        ASSERT_TRUE(fixture.run_test(test));                                                                           \
    }

#define _OBLO_SMOKE_ASSERT_IMPL(Check, ...)                                                                            \
    EXPECT_##Check(__VA_ARGS__);                                                                                       \
                                                                                                                       \
    if (g_currentTest->HasFailure())                                                                                   \
    {                                                                                                                  \
        co_return;                                                                                                     \
    }

#define OBLO_SMOKE_TRUE(...) _OBLO_SMOKE_ASSERT_IMPL(TRUE, __VA_ARGS__)
#define OBLO_SMOKE_FALSE(...) _OBLO_SMOKE_ASSERT_IMPL(FALSE, __VA_ARGS__)
#define OBLO_SMOKE_EQ(...) _OBLO_SMOKE_ASSERT_IMPL(EQ, __VA_ARGS__)
#define OBLO_SMOKE_NE(...) _OBLO_SMOKE_ASSERT_IMPL(NE, __VA_ARGS__)
