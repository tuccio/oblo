#pragma once

#include <memory>

namespace oblo::smoke
{
    class test;

    struct test_fixture_config
    {
        const char* name{};
        void (*onCompletion)(void*){};
    };

    class test_fixture
    {
    public:
        test_fixture();
        test_fixture(const test_fixture&) = delete;
        test_fixture(test_fixture&&) noexcept = delete;
        ~test_fixture();

        test_fixture& operator=(const test_fixture&) = delete;
        test_fixture& operator=(test_fixture&&) noexcept = delete;

        [[nodiscard]] bool init(const test_fixture_config& cfg);

        [[nodiscard]] bool run_test(test& test);

        void run_interactive();

    private:
        struct impl;

    private:
        std::unique_ptr<impl> m_impl;
    };
}