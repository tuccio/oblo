#include <oblo/core/time/clock.hpp>

#include <chrono>

namespace oblo::clock
{
    using stl_clock = std::chrono::high_resolution_clock;
    using stl_hns = std::ratio<1, 10'000'000>;
    using stl_duration = std::chrono::duration<i64, stl_hns>;

    time now()
    {
        const auto n = stl_clock::now();
        const auto d = n.time_since_epoch();
        const auto hns = std::chrono::duration_cast<stl_duration>(d);
        return {hns.count()};
    }
}