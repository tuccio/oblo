#include <gtest/gtest.h>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/deque.hpp>

#include <span>
#include <unordered_map>
#include <vector>

namespace oblo
{
    struct checked_allocator final : allocator
    {
        byte* allocate(usize size, usize alignment) noexcept override
        {
            auto* ptr = upstream->allocate(size, alignment);
            allocations.emplace(ptr, allocation_info{size, alignment});

            return ptr;
        }

        void deallocate(byte* ptr, usize size, usize alignment) noexcept override
        {
            const auto it = allocations.find(ptr);
            ASSERT_NE(it, allocations.end());

            const auto& info = it->second;
            ASSERT_EQ(info.size, size);
            ASSERT_EQ(info.alignment, alignment);

            allocations.erase(it);

            upstream->deallocate(ptr, size, alignment);
        }

        allocator* upstream{get_global_allocator()};

        struct allocation_info
        {
            usize size;
            usize alignment;
        };

        std::unordered_map<void*, allocation_info> allocations;
    };

    TEST(deque, deque_trivial)
    {
        checked_allocator allocator;

        {
            deque<i32> queue{&allocator, deque_config{.elementsPerChunk = 32}, 0};
            std::vector<i32> expected;

            ASSERT_EQ(queue.size(), 0);
            ASSERT_EQ(queue.capacity(), 0);
            ASSERT_EQ(queue.elements_per_chunk(), 32);

            for (i32 i = 0; i < 1024; ++i)
            {
                queue.emplace_back(i);
                expected.emplace_back(i);

                if (i % 32 == 0)
                {
                    queue.shrink_to_fit();
                }

                ASSERT_EQ(queue.size(), i + 1);
                ASSERT_GE(queue.capacity(), i + 1);

                ASSERT_EQ(queue.size(), expected.size());

                ASSERT_TRUE(std::equal(queue.begin(), queue.end(), expected.begin()));

                const auto expectedAllocations = 1 + queue.capacity() / queue.elements_per_chunk();

                ASSERT_EQ(allocator.allocations.size(), expectedAllocations);

                deque<i32> copy{queue};
                ASSERT_EQ(allocator.allocations.size(), 2 * expectedAllocations);

                ASSERT_EQ(queue, copy);
            }

            const auto expectedAllocations = 1 + queue.capacity() / queue.elements_per_chunk();
            deque<i32> copy{queue};
            ASSERT_EQ(allocator.allocations.size(), 2 * expectedAllocations);

            ASSERT_EQ(queue, copy);

            deque<i32> move{std::move(copy)};
            ASSERT_EQ(allocator.allocations.size(), 2 * expectedAllocations);

            ASSERT_EQ(queue, move);
        }

        ASSERT_EQ(allocator.allocations.size(), 0);
    }

    namespace
    {
        class move_only_int
        {
        public:
            explicit move_only_int(i32 v) : m_value{std::make_unique<i32>(v)} {}

            move_only_int(move_only_int&&) noexcept = default;

            bool operator==(const move_only_int& other) const
            {
                return *m_value == *other.m_value;
            }

        private:
            std::unique_ptr<i32> m_value;
        };
    }

    TEST(deque, deque_move_only)
    {
        checked_allocator allocator;

        {
            deque<move_only_int> queue{&allocator, deque_config{.elementsPerChunk = 32}};
            std::vector<move_only_int> expected;

            ASSERT_EQ(queue.size(), 0);
            ASSERT_EQ(queue.capacity(), 0);
            ASSERT_EQ(queue.elements_per_chunk(), 32);

            for (i32 i = 0; i < 1024; ++i)
            {
                queue.emplace_back(i);
                expected.emplace_back(i);

                if (i % 32 == 0)
                {
                    queue.shrink_to_fit();
                }

                ASSERT_EQ(queue.size(), i + 1);
                ASSERT_GE(queue.capacity(), i + 1);

                ASSERT_EQ(queue.size(), expected.size());

                ASSERT_TRUE(std::equal(queue.begin(), queue.end(), expected.begin()));

                const auto expectedAllocations = 1 + queue.capacity() / queue.elements_per_chunk();

                ASSERT_EQ(allocator.allocations.size(), expectedAllocations);
            }

            const auto expectedAllocations = 1 + queue.capacity() / queue.elements_per_chunk();

            deque<move_only_int> move{std::move(queue)};
            ASSERT_EQ(allocator.allocations.size(), expectedAllocations);

            ASSERT_EQ(move.size(), expected.size());
            ASSERT_TRUE(std::equal(move.begin(), move.end(), expected.begin()));

            // Move it back
            queue = std::move(move);

            ASSERT_EQ(allocator.allocations.size(), expectedAllocations);

            ASSERT_EQ(queue.size(), expected.size());
            ASSERT_TRUE(std::equal(queue.begin(), queue.end(), expected.begin()));
        }

        ASSERT_EQ(allocator.allocations.size(), 0);
    }

    // struct aligned32_value
    //{
    //     alignas(32) u32 values[16];
    // };

    // TEST(deque, deque_alignment)
    //{
    //     deque<aligned32_value> array;
    //     ASSERT_EQ(array.data(), nullptr);

    //    array = {aligned32_value{{1}}, aligned32_value{{2}}, aligned32_value{{3}}};

    //    ASSERT_NE(array.data(), nullptr);
    //    ASSERT_EQ(uintptr(array.data()) % 32, 0);

    //    ASSERT_EQ(array.size(), 3);

    //    ASSERT_EQ(array.data(), &array[0]);

    //    ASSERT_EQ(array[0].values[0], 1);
    //    ASSERT_EQ(array[1].values[0], 2);
    //    ASSERT_EQ(array[2].values[0], 3);
    //}

    // TEST(deque, deque_insert_range)
    //{
    //     deque<i32> array;

    //    array = {1, 2, 3, 7, 8, 9, 10};

    //    constexpr i32 elements[] = {4, 5, 6};

    //    ASSERT_EQ(array.size(), 7);

    //    const auto it = array.begin() + 3;
    //    ASSERT_EQ(*it, 7);

    //    const auto newIt = array.insert(it, std::begin(elements), std::end(elements));
    //    ASSERT_EQ(*newIt, 4);

    //    ASSERT_EQ(array.size(), 10);

    //    for (i32 i = 0; i < 10; ++i)
    //    {
    //        ASSERT_EQ(array[i], i + 1);
    //    }
    //}

    // TEST(deque, deque_insert_single_elements)
    //{
    //     deque<i32> array;

    //    array = {1, 2, 3, 7, 8, 9, 10};

    //    ASSERT_EQ(array.size(), 7);

    //    const auto it = array.begin() + 3;
    //    ASSERT_EQ(*it, 7);

    //    for (i32 i = 4; i <= 6; ++i)
    //    {
    //        const auto newIt = array.insert(array.begin() + i - 1, i);
    //        ASSERT_EQ(*newIt, i);

    //        ASSERT_TRUE(std::is_sorted(array.begin(), array.end()));
    //    }

    //    ASSERT_EQ(array.size(), 10);

    //    for (i32 i = 0; i < 10; ++i)
    //    {
    //        ASSERT_EQ(array[i], i + 1);
    //    }

    //    array.insert(array.end(), 11);
    //    array.insert(array.end(), 12);
    //    array.insert(array.end(), 13);

    //    for (i32 i = 0; i < 13; ++i)
    //    {
    //        ASSERT_EQ(array[i], i + 1);
    //    }
    //}

    // TEST(deque, deque_erase_range)
    //{
    //     deque<i32> array;

    //    array = {1, 2, 0, 0, 3, 4, 5, 42, 42};

    //    ASSERT_EQ(array.size(), 9);

    //    const auto it = array.begin() + 3;
    //    ASSERT_EQ(*it, 0);

    //    const auto newIt = array.erase(array.begin() + 2, array.begin() + 4);
    //    ASSERT_EQ(*newIt, 3);

    //    ASSERT_EQ(array.size(), 7);

    //    for (i32 i = 0; i < 5; ++i)
    //    {
    //        ASSERT_EQ(array[i], i + 1);
    //    }

    //    for (i32 i = 6; i < 7; ++i)
    //    {
    //        ASSERT_EQ(array[i], 42);
    //    }

    //    const auto endIt = array.erase(array.begin() + 5, array.end());
    //    ASSERT_EQ(endIt, array.end());

    //    ASSERT_EQ(array.size(), 5);

    //    for (i32 i = 0; i < 5; ++i)
    //    {
    //        ASSERT_EQ(array[i], i + 1);
    //    }
    //}

    // TEST(deque, deque_erase_single_element)
    //{
    //     deque<i32> array;

    //    array = {1, 2, -1, -1, 3, 4, 5, -1, -1};

    //    ASSERT_EQ(array.size(), 9);

    //    u32 iterations{0};

    //    for (auto it = array.begin(); it != array.end();)
    //    {
    //        ++iterations;

    //        if (*it < 0)
    //        {
    //            it = array.erase(it);
    //        }
    //        else
    //        {
    //            ++it;
    //        }
    //    }

    //    ASSERT_EQ(array.size(), 5);
    //    ASSERT_EQ(iterations, 9);

    //    for (i32 i = 0; i < 5; ++i)
    //    {
    //        ASSERT_EQ(array[i], i + 1);
    //    }
    //}

    // TEST(deque, deque_erase_unordered)
    //{
    //     deque<i32> array;

    //    array = {1, 2, -1, -1, 3, 4, 5, -1, -1};

    //    ASSERT_EQ(array.size(), 9);

    //    u32 iterations{0};

    //    for (auto it = array.begin(); it != array.end();)
    //    {
    //        ++iterations;

    //        if (*it < 0)
    //        {
    //            it = array.erase_unordered(it);
    //        }
    //        else
    //        {
    //            ++it;
    //        }
    //    }

    //    ASSERT_EQ(array.size(), 5);
    //    ASSERT_EQ(iterations, 9);

    //    std::sort(array.begin(), array.end());

    //    for (i32 i = 0; i < 5; ++i)
    //    {
    //        ASSERT_EQ(array[i], i + 1);
    //    }
    //}

    // TEST(buffered_array, buffered_array_trivial)
    //{
    //     checked_allocator allocator;

    //    {
    //        constexpr auto N = 16;
    //        constexpr auto M = 1024;

    //        buffered_array<i32, N> array{&allocator};
    //        std::vector<i32> expected;

    //        ASSERT_EQ(array.size(), 0);
    //        ASSERT_EQ(array.capacity(), N);

    //        for (i32 i = 0; i < M; ++i)
    //        {
    //            array.emplace_back(i);
    //            expected.emplace_back(i);

    //            if (i % 32 == 0)
    //            {
    //                array.shrink_to_fit();
    //            }

    //            ASSERT_EQ(array.size(), i + 1);
    //            ASSERT_GE(array.capacity(), i + 1);

    //            ASSERT_EQ(array.size(), expected.size());

    //            ASSERT_TRUE(std::equal(array.begin(), array.end(), expected.begin()));

    //            if (i < N)
    //            {
    //                ASSERT_EQ(allocator.allocations.size(), 0);
    //            }
    //            else
    //            {
    //                ASSERT_EQ(allocator.allocations.size(), 1);
    //            }
    //        }

    //        {
    //            deque<i32> copy{array};
    //            ASSERT_EQ(allocator.allocations.size(), 2);

    //            ASSERT_EQ(array, copy);

    //            deque<i32> move{std::move(copy)};
    //            ASSERT_EQ(allocator.allocations.size(), 2);
    //        }

    //        {
    //            buffered_array<i32, M> copy{array};
    //            ASSERT_EQ(allocator.allocations.size(), 1);

    //            ASSERT_EQ(array, copy);

    //            buffered_array<i32, M> move{std::move(copy)};
    //            ASSERT_EQ(allocator.allocations.size(), 1);
    //        }
    //    }

    //    ASSERT_EQ(allocator.allocations.size(), 0);
    //}
}