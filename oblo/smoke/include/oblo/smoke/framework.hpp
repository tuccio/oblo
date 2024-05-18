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

namespace oblo::smoke
{
    inline thread_local testing::Test* g_currentTest{};
    inline bool g_interactiveMode{};
}

#define OBLO_SMOKE_TEST(Class)                                                                                         \
    TEST(smoke_test, Class)                                                                                            \
    {                                                                                                                  \
        ::oblo::smoke::g_currentTest = this;                                                                           \
        const auto cleanup = finally([] { ::oblo::smoke::g_currentTest = nullptr; });                                  \
                                                                                                                       \
        Class test;                                                                                                    \
        ::oblo::smoke::test_fixture fixture;                                                                           \
                                                                                                                       \
        const ::oblo::smoke::test_fixture_config cfg{                                                                  \
            .name = #Class,                                                                                            \
            .interactiveMode = ::oblo::smoke::g_interactiveMode,                                                       \
        };                                                                                                             \
                                                                                                                       \
        ASSERT_TRUE(fixture.run_test(test, cfg));                                                                      \
    }

#define _OBLO_SMOKE_ASSERT_IMPL(Check, ...)                                                                            \
    EXPECT_##Check(__VA_ARGS__);                                                                                       \
                                                                                                                       \
    if (::oblo::smoke::g_currentTest->HasFailure())                                                                    \
    {                                                                                                                  \
        co_return;                                                                                                     \
    }

#define OBLO_SMOKE_TRUE(...) _OBLO_SMOKE_ASSERT_IMPL(TRUE, __VA_ARGS__)
#define OBLO_SMOKE_FALSE(...) _OBLO_SMOKE_ASSERT_IMPL(FALSE, __VA_ARGS__)
#define OBLO_SMOKE_EQ(...) _OBLO_SMOKE_ASSERT_IMPL(EQ, __VA_ARGS__)
#define OBLO_SMOKE_NE(...) _OBLO_SMOKE_ASSERT_IMPL(NE, __VA_ARGS__)
