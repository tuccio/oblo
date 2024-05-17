#pragma once

namespace oblo::smoke
{
    class test;

    class test_fixture
    {
    public:
        [[nodiscard]] bool run_test(test& test);
    };
}