#pragma once

namespace oblo::smoke
{
    class test_task;
    class test_context;

    class test
    {
    public:
        virtual ~test() = default;

        virtual test_task run(const test_context& ctx) = 0;
    };
}