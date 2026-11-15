#include <oblo/core/time/clock.hpp>

#include <Windows.h>

namespace oblo::clock
{
    void sleep(time duration)
    {
        if (duration.hns <= 0)
        {
            return;
        }

        // Round up, since Sleep only has 1ms granularity
        Sleep(DWORD((duration.hns + 9'999) / 10'000));
    }
}
