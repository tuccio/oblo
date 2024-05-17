#pragma once

#include <gtest/gtest.h>

namespace oblo::smoke
{
    class test;

    class test_fixture
    {
    public:
        [[nodiscard]] bool run_test(test& test);
    };
}