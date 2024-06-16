#pragma once

#include <oblo/core/utility.hpp>
#include <oblo/thread/job_manager.hpp>

namespace oblo
{
    struct job_range
    {
        u32 begin;
        u32 end;
    };

    template <typename F>
    void parallel_for(F&& f, const job_range range, u32 granularity = 32)
    {
        if (range.begin == range.end)
        {
            return;
        }

        job_manager& jm = *job_manager::get();

        const job_range firstJobRange{
            .begin = 0,
            .end = min(granularity, range.end),
        };

        // We are passing a reference to the function here because the function is blocking and stays alive
        const job_handle firstJob = jm.push_waitable([&f, firstJobRange]() { f(firstJobRange); });

        for (u32 b = range.begin + granularity; b != range.end; b += granularity)
        {
            const job_range childRange{
                .begin = b,
                .end = min(b + granularity, range.end),
            };

            jm.push_child(firstJob, [&f, childRange]() { f(childRange); });
        }

        jm.wait(firstJob);
    }
}