#include <gtest/gtest.h>

#include <oblo/core/ring_buffer.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    TEST(ring_buffer, no_grow)
    {
        constexpr auto N{4};

        ring_buffer<i32> buffer{N};

        {
            ASSERT_TRUE(buffer.has_available(N));
            const auto usedSegmentsEmpty = buffer.used_segments();
            ASSERT_EQ(usedSegmentsEmpty.firstSegmentBegin, usedSegmentsEmpty.firstSegmentEnd);
            ASSERT_EQ(usedSegmentsEmpty.secondSegmentBegin, usedSegmentsEmpty.secondSegmentEnd);

            const auto result = buffer.fetch(N);
            ASSERT_EQ(result.firstSegmentEnd - result.firstSegmentBegin, N);
            ASSERT_EQ(result.secondSegmentEnd - result.secondSegmentBegin, 0);

            for (i32 i = 0; i < N; ++i)
            {
                result.firstSegmentBegin[i] = i;
            }

            const auto usedSegmentsFull = buffer.used_segments();
            ASSERT_EQ(usedSegmentsFull.firstSegmentEnd - usedSegmentsFull.firstSegmentBegin, N);
            ASSERT_EQ(usedSegmentsFull.secondSegmentBegin, usedSegmentsFull.secondSegmentEnd);

            for (i32 i = 0; i < N; ++i)
            {
                usedSegmentsFull.firstSegmentEnd[i] = i;
            }

            ASSERT_FALSE(buffer.has_available(1));
            buffer.release(N);
            ASSERT_TRUE(buffer.has_available(1));
            ASSERT_TRUE(buffer.has_available(N));
        }

        for (int j = 0; j < N; ++j)
        {
            ASSERT_TRUE(buffer.has_available(N));

            for (int i = 0; i < N; ++i)
            {
                const auto result = buffer.fetch(1);
                ASSERT_EQ(result.firstSegmentEnd - result.firstSegmentBegin, 1);
                ASSERT_EQ(result.secondSegmentEnd - result.secondSegmentBegin, 0);
                ASSERT_EQ(*result.firstSegmentBegin, i);

                ASSERT_FALSE(buffer.has_available(N - i));
                ASSERT_TRUE(buffer.has_available(N - i - 1));

                const auto usedSegments = buffer.used_segments();
                ASSERT_EQ(usedSegments.firstSegmentEnd - usedSegments.firstSegmentBegin, i + 1);
                ASSERT_EQ(usedSegments.secondSegmentBegin, usedSegments.secondSegmentEnd);

                for (int j = 0; j < N; ++j)
                {
                    ASSERT_EQ(usedSegments.firstSegmentBegin[j], j);
                }
            }

            ASSERT_FALSE(buffer.has_available(1));

            for (int i = 0; i < N; ++i)
            {
                ASSERT_TRUE(buffer.has_available(i));
                ASSERT_FALSE(buffer.has_available(i + 1));

                buffer.release(1);
                ASSERT_TRUE(buffer.has_available(i + 1));

                const auto usedSegments = buffer.used_segments();
                ASSERT_EQ(usedSegments.firstSegmentEnd - usedSegments.firstSegmentBegin, N - i - 1);
                ASSERT_EQ(usedSegments.secondSegmentBegin, usedSegments.secondSegmentEnd);

                for (int j = 0; j < N - i - 1; ++j)
                {
                    ASSERT_EQ(usedSegments.firstSegmentBegin[j], i + j + 1);
                }
            }
        }

        {
            ASSERT_TRUE(buffer.has_available(N));

            const auto resultHalfN = buffer.fetch(N / 2);
            ASSERT_EQ(resultHalfN.firstSegmentEnd - resultHalfN.firstSegmentBegin, N / 2);
            ASSERT_EQ(resultHalfN.secondSegmentEnd - resultHalfN.secondSegmentBegin, 0);

            for (i32 i = 0; i < N / 2; ++i)
            {
                ASSERT_EQ(resultHalfN.firstSegmentBegin[i], i);
            }

            const auto usedHalfSegments = buffer.used_segments();
            ASSERT_EQ(usedHalfSegments.firstSegmentEnd - usedHalfSegments.firstSegmentBegin, N / 2);
            ASSERT_EQ(usedHalfSegments.secondSegmentBegin, usedHalfSegments.secondSegmentEnd);

            ASSERT_FALSE(buffer.has_available(N));
            buffer.release(N / 2);

            ASSERT_TRUE(buffer.has_available(N));

            const auto resultN = buffer.fetch(N);
            ASSERT_EQ(resultN.firstSegmentEnd - resultN.firstSegmentBegin, N / 2);
            ASSERT_EQ(resultN.secondSegmentEnd - resultN.secondSegmentBegin, N / 2);

            const auto usedFullSegments = buffer.used_segments();
            ASSERT_EQ(usedFullSegments.firstSegmentEnd - usedFullSegments.firstSegmentBegin, N / 2);
            ASSERT_EQ(usedFullSegments.secondSegmentEnd - usedFullSegments.secondSegmentBegin, N / 2);

            for (i32 i = 0; i < N / 2; ++i)
            {
                ASSERT_EQ(resultN.firstSegmentBegin[i], N / 2 + i);
                ASSERT_EQ(resultN.secondSegmentBegin[i], i);
            }
        }
    }
}