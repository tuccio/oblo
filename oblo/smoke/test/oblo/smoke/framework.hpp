#pragma once

#include <oblo/smoke/framework/test.hpp>
#include <oblo/smoke/framework/test_context.hpp>
#include <oblo/smoke/framework/test_fixture.hpp>
#include <oblo/smoke/framework/test_task.hpp>

#define OBLO_SMOKE_TEST(Class)                                                                                         \
    TEST(smoke_test, Class)                                                                                            \
    {                                                                                                                  \
        Class test;                                                                                                    \
        ::oblo::smoke::test_fixture fixture;                                                                           \
        ASSERT_TRUE(fixture.run_test(test));                                                                           \
    }