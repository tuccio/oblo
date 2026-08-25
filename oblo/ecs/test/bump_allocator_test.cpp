#include <gtest/gtest.h>

#include <oblo/core/bump_allocator.hpp>

#include <cstdint>

namespace oblo
{
    class bump_allocator_test : public ::testing::Test
    {
    protected:
        static constexpr usize chunk_size = 256;

        bump_allocator allocator{chunk_size};
    };

    TEST_F(bump_allocator_test, AllocatesMemory)
    {
        byte* ptr = allocator.allocate(16, 8);

        ASSERT_NE(ptr, nullptr);
    }

    TEST_F(bump_allocator_test, ReturnsAlignedMemory)
    {
        constexpr usize alignments[] = {
            1,
            2,
            4,
            8,
            16,
            32,
            64,
        };

        for (usize alignment : alignments)
        {
            byte* ptr = allocator.allocate(1, alignment);

            ASSERT_NE(ptr, nullptr);
            EXPECT_EQ(reinterpret_cast<uintptr>(ptr) & (alignment - 1), 0u);
        }
    }

    TEST_F(bump_allocator_test, AllocationsDoNotOverlap)
    {
        byte* first = allocator.allocate(32, 8);
        byte* second = allocator.allocate(32, 8);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);

        EXPECT_GE(second, first + 32);
    }

    TEST_F(bump_allocator_test, AllocationsAreSequential)
    {
        byte* first = allocator.allocate(16, 1);
        byte* second = allocator.allocate(16, 1);
        byte* third = allocator.allocate(16, 1);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        ASSERT_NE(third, nullptr);

        EXPECT_EQ(second, first + 16);
        EXPECT_EQ(third, second + 16);
    }

    TEST_F(bump_allocator_test, AlignmentIntroducesPadding)
    {
        byte* first = allocator.allocate(1, 1);
        byte* second = allocator.allocate(1, 16);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(second) % 16, 0u);

        EXPECT_GT(second, first);
    }

    TEST_F(bump_allocator_test, SupportsLargeAlignment)
    {
        constexpr usize alignment = 256;

        byte* ptr = allocator.allocate(32, alignment);

        ASSERT_NE(ptr, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(ptr) % alignment, 0u);
    }

    TEST_F(bump_allocator_test, AllocatesAcrossChunks)
    {
        byte* first = allocator.allocate(128, 8);
        byte* second = allocator.allocate(128, 8);
        byte* third = allocator.allocate(128, 8);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        ASSERT_NE(third, nullptr);

        EXPECT_NE(first, second);
        EXPECT_NE(second, third);
        EXPECT_NE(first, third);
    }

    TEST_F(bump_allocator_test, LargeAllocationGetsItsOwnChunk)
    {
        constexpr usize size = chunk_size * 2;

        byte* ptr = allocator.allocate(size, 16);

        ASSERT_NE(ptr, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(ptr) % 16, 0u);
    }

    TEST_F(bump_allocator_test, ResetAllowsMemoryToBeReused)
    {
        byte* first = allocator.allocate(32, 8);

        ASSERT_NE(first, nullptr);

        allocator.reset();

        byte* second = allocator.allocate(32, 8);

        ASSERT_NE(second, nullptr);

        EXPECT_EQ(second, first);
    }

    TEST_F(bump_allocator_test, ResetWorksAcrossMultipleChunks)
    {
        byte* first = allocator.allocate(128, 8);
        byte* second = allocator.allocate(128, 8);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);

        allocator.reset();

        byte* firstAfterReset = allocator.allocate(128, 8);
        byte* secondAfterReset = allocator.allocate(128, 8);

        ASSERT_NE(firstAfterReset, nullptr);
        ASSERT_NE(secondAfterReset, nullptr);

        EXPECT_EQ(firstAfterReset, first);
        EXPECT_EQ(secondAfterReset, second);
    }

    TEST_F(bump_allocator_test, DeallocateDoesNothing)
    {
        byte* first = allocator.allocate(32, 8);

        ASSERT_NE(first, nullptr);

        allocator.deallocate(first, 32, 8);

        byte* second = allocator.allocate(32, 8);

        ASSERT_NE(second, nullptr);
        EXPECT_NE(second, first);
    }

    TEST_F(bump_allocator_test, ZeroSizeAllocation)
    {
        byte* ptr = allocator.allocate(0, 8);

        ASSERT_NE(ptr, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(ptr) % 8, 0u);
    }

    TEST_F(bump_allocator_test, RejectsZeroAlignment)
    {
        EXPECT_EQ(allocator.allocate(16, 0), nullptr);
    }

    TEST_F(bump_allocator_test, RejectsNonPowerOfTwoAlignment)
    {
        EXPECT_EQ(allocator.allocate(16, 3), nullptr);

        EXPECT_EQ(allocator.allocate(16, 6), nullptr);

        EXPECT_EQ(allocator.allocate(16, 12), nullptr);
    }

    TEST_F(bump_allocator_test, AcceptsPowerOfTwoAlignments)
    {
        constexpr usize alignments[] = {
            1,
            2,
            4,
            8,
            16,
            32,
            64,
            128,
            256,
        };

        for (usize alignment : alignments)
        {
            byte* ptr = allocator.allocate(1, alignment);

            ASSERT_NE(ptr, nullptr) << "alignment = " << alignment;

            EXPECT_EQ(reinterpret_cast<uintptr>(ptr) & (alignment - 1), 0u);
        }
    }

    TEST_F(bump_allocator_test, AllocatesAfterResetWithDifferentAlignments)
    {
        allocator.allocate(13, 1);
        allocator.allocate(27, 8);
        allocator.allocate(7, 16);

        allocator.reset();

        constexpr usize alignment = 64;

        byte* ptr = allocator.allocate(32, alignment);

        ASSERT_NE(ptr, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(ptr) & (alignment - 1), 0u);
    }
}
