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

    TEST_F(bump_allocator_test, allocates_memory)
    {
        byte* ptr = allocator.allocate(16, 8);

        ASSERT_NE(ptr, nullptr);
    }

    TEST_F(bump_allocator_test, returns_aligned_memory)
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

    TEST_F(bump_allocator_test, allocations_do_not_overlap)
    {
        byte* first = allocator.allocate(32, 8);
        byte* second = allocator.allocate(32, 8);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);

        EXPECT_GE(second, first + 32);
    }

    TEST_F(bump_allocator_test, allocations_are_sequential)
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

    TEST_F(bump_allocator_test, alignment_introduces_padding)
    {
        byte* first = allocator.allocate(1, 1);
        byte* second = allocator.allocate(1, 16);

        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(second) % 16, 0u);

        EXPECT_GT(second, first);
    }

    TEST_F(bump_allocator_test, supports_large_alignment)
    {
        constexpr usize alignment = 256;

        byte* ptr = allocator.allocate(32, alignment);

        ASSERT_NE(ptr, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(ptr) % alignment, 0u);
    }

    TEST_F(bump_allocator_test, allocates_across_chunks)
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

    TEST_F(bump_allocator_test, large_allocation_gets_its_own_chunk)
    {
        constexpr usize size = chunk_size * 2;

        byte* ptr = allocator.allocate(size, 16);

        ASSERT_NE(ptr, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(ptr) % 16, 0u);
    }

    TEST_F(bump_allocator_test, reset_allows_memory_to_be_reused)
    {
        byte* first = allocator.allocate(32, 8);

        ASSERT_NE(first, nullptr);

        allocator.reset();

        byte* second = allocator.allocate(32, 8);

        ASSERT_NE(second, nullptr);

        EXPECT_EQ(second, first);
    }

    TEST_F(bump_allocator_test, reset_works_across_multiple_chunks)
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

    TEST_F(bump_allocator_test, deallocate_does_nothing)
    {
        byte* first = allocator.allocate(32, 8);

        ASSERT_NE(first, nullptr);

        allocator.deallocate(first, 32, 8);

        byte* second = allocator.allocate(32, 8);

        ASSERT_NE(second, nullptr);
        EXPECT_NE(second, first);
    }

    TEST_F(bump_allocator_test, zero_size_allocation)
    {
        byte* ptr = allocator.allocate(0, 8);

        ASSERT_NE(ptr, nullptr);

        EXPECT_EQ(reinterpret_cast<uintptr>(ptr) % 8, 0u);
    }

    TEST_F(bump_allocator_test, accepts_power_of_two_alignments)
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

    TEST_F(bump_allocator_test, allocates_after_reset_with_different_alignments)
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
