#pragma once

namespace oblo::smoke
{
    class test;

    struct test_fixture_config
    {
        const char* name{};
        bool interactiveMode{};
    };

    class test_fixture
    {
    public:
        [[nodiscard]] bool run_test(test& test, const test_fixture_config& cfg);
    };
}