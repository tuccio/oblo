#include <gtest/gtest.h>

#include <oblo/core/time/time.hpp>

#include <chrono>

namespace oblo
{
    using f32_seconds = std::chrono::duration<f32>;

    using hns_ratio = std::ratio<1, 10'000'000>;
    using duration_hns = std::chrono::duration<i64, hns_ratio>;

    TEST(time, from_seconds)
    {
        constexpr f32_seconds durations[] = {
            f32_seconds(0),
            f32_seconds(1),
            f32_seconds(60),
            f32_seconds(3600),
            f32_seconds(86400),
            f32_seconds(-1),
            f32_seconds(-60),
            f32_seconds(-3600),
            f32_seconds(-86400),
        };

        for (const auto& duration : durations)
        {
            const time t = time::from_seconds(duration.count());
            const auto hns = std::chrono::duration_cast<duration_hns>(duration);

            ASSERT_EQ(hns.count(), t.hns);
        }
    }
}