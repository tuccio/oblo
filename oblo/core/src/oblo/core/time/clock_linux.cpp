#include <oblo/core/time/clock.hpp>
#include <oblo/core/types.hpp>

#include <time.h>

namespace oblo::clock
{
    void sleep(time duration)
    {
        if (duration.hns <= 0)
        {
            return;
        }

        // The unit of time is 100 nanoseconds
        const i64 seconds = duration.hns / 10'000'000;
        const i64 remainder = duration.hns % 10'000'000;

        const timespec ts{.tv_sec = time_t(seconds), .tv_nsec = long(remainder) * 100};
        nanosleep(&ts, nullptr);
    }
}
