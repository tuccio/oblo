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
    void parallel_for(F&& f, const job_range range, u32 granularity)
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

        for (u32 b = range.begin + granularity; b < range.end; b += granularity)
        {
            const job_range childRange{
                .begin = b,
                .end = min(b + granularity, range.end),
            };

            jm.push_child(firstJob, [&f, childRange]() { f(childRange); });
        }

        // Use a single wait function that will keep picking up jobs
        jm.wait(firstJob);
    }

    template <typename F>
    void parallel_for_2d(F&& f, const job_range rows, const job_range columns, u32 rowGranularity, u32 colGranularity)
    {
        if (rows.begin == rows.end || columns.begin == columns.end)
        {
            return;
        }

        job_manager& jm = *job_manager::get();

        const job_range firstJobRows{
            .begin = 0,
            .end = min(rowGranularity, rows.end),
        };

        const job_range firstJobColumns{
            .begin = 0,
            .end = min(colGranularity, columns.end),
        };

        // We are passing a reference to the function here because the function is blocking and stays alive
        const job_handle firstJob =
            jm.push_waitable([&f, firstJobRows, firstJobColumns]() { f(firstJobRows, firstJobColumns); });

        // Complete the first row
        for (u32 c = columns.begin + colGranularity; c < columns.end; c += colGranularity)
        {
            const job_range childCols{
                .begin = c,
                .end = min(c + colGranularity, columns.end),
            };

            jm.push_child(firstJob, [&f, firstJobRows, childCols]() { f(firstJobRows, childCols); });
        }

        // Now do the rest
        for (u32 r = rows.begin + rowGranularity; r < rows.end; r += rowGranularity)
        {
            const job_range childRows{
                .begin = r,
                .end = min(r + rowGranularity, rows.end),
            };

            for (u32 c = columns.begin; c < columns.end; c += colGranularity)
            {
                const job_range childCols{
                    .begin = c,
                    .end = min(c + colGranularity, columns.end),
                };

                jm.push_child(firstJob, [&f, childRows, childCols]() { f(childRows, childCols); });
            }
        }

        // Use a single wait function that will keep picking up jobs
        jm.wait(firstJob);
    }
}