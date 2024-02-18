#include <gtest/gtest.h>

#include <oblo/core/dynamic_array.hpp>

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

    TEST(dynamic_array, dynamic_array_trivial)
    {
        checked_allocator allocator;

        {
            dynamic_array<i32> array{&allocator};
            std::vector<i32> expected;

            ASSERT_EQ(array.size(), 0);
            ASSERT_EQ(array.capacity(), 0);

            for (i32 i = 0; i < 1024; ++i)
            {
                array.emplace_back(i);
                expected.emplace_back(i);

                if (i % 32 == 0)
                {
                    array.shrink_to_fit();
                }

                ASSERT_EQ(array.size(), i + 1);
                ASSERT_GE(array.capacity(), i + 1);

                ASSERT_EQ(array.size(), expected.size());

                ASSERT_TRUE(std::equal(array.begin(), array.end(), expected.begin()));

                ASSERT_EQ(allocator.allocations.size(), 1);
            }

            dynamic_array<i32> copy{array};
            ASSERT_EQ(allocator.allocations.size(), 2);

            ASSERT_EQ(array, copy);

            dynamic_array<i32> move{std::move(copy)};
            ASSERT_EQ(allocator.allocations.size(), 2);
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

    TEST(dynamic_array, dynamic_array_move_only)
    {
        checked_allocator allocator;

        {
            dynamic_array<move_only_int> array{&allocator};
            std::vector<move_only_int> expected;

            ASSERT_EQ(array.size(), 0);
            ASSERT_EQ(array.capacity(), 0);

            for (i32 i = 0; i < 1024; ++i)
            {
                array.emplace_back(i);
                expected.emplace_back(i);

                if (i % 32 == 0)
                {
                    array.shrink_to_fit();
                }

                ASSERT_EQ(array.size(), i + 1);
                ASSERT_GE(array.capacity(), i + 1);

                ASSERT_EQ(array.size(), expected.size());

                ASSERT_TRUE(std::equal(array.begin(), array.end(), expected.begin()));

                ASSERT_EQ(allocator.allocations.size(), 1);
            }

            dynamic_array<move_only_int> move{std::move(array)};
            ASSERT_EQ(allocator.allocations.size(), 1);

            ASSERT_EQ(move.size(), expected.size());
            ASSERT_TRUE(std::equal(move.begin(), move.end(), expected.begin()));

            // Move it back
            array = std::move(move);
            ASSERT_EQ(allocator.allocations.size(), 1);

            ASSERT_EQ(array.size(), expected.size());
            ASSERT_TRUE(std::equal(array.begin(), array.end(), expected.begin()));
        }

        ASSERT_EQ(allocator.allocations.size(), 0);
    }

    struct aligned32_value
    {
        alignas(32) u32 values[16];
    };

    TEST(dynamic_array, dynamic_array_alignment)
    {
        dynamic_array<aligned32_value> array;
        ASSERT_EQ(array.data(), nullptr);

        array = {aligned32_value{{1}}, aligned32_value{{2}}, aligned32_value{{3}}};

        ASSERT_NE(array.data(), nullptr);
        ASSERT_EQ(uintptr(array.data()) % 32, 0);

        ASSERT_EQ(array.size(), 3);

        ASSERT_EQ(array.data(), &array[0]);

        ASSERT_EQ(array[0].values[0], 1);
        ASSERT_EQ(array[1].values[0], 2);
        ASSERT_EQ(array[2].values[0], 3);
    }

    TEST(dynamic_array, dynamic_array_insert_range)
    {
        dynamic_array<i32> array;

        array = {1, 2, 3, 7, 8, 9, 10};

        constexpr i32 elements[] = {4, 5, 6};

        ASSERT_EQ(array.size(), 7);

        const auto it = array.begin() + 3;
        ASSERT_EQ(*it, 7);

        const auto newIt = array.insert(it, std::begin(elements), std::end(elements));
        ASSERT_EQ(*newIt, 4);

        ASSERT_EQ(array.size(), 10);

        for (i32 i = 0; i < 10; ++i)
        {
            ASSERT_EQ(array[i], i + 1);
        }
    }

    TEST(dynamic_array, dynamic_array_insert_single_elements)
    {
        dynamic_array<i32> array;

        array = {1, 2, 3, 7, 8, 9, 10};

        ASSERT_EQ(array.size(), 7);

        const auto it = array.begin() + 3;
        ASSERT_EQ(*it, 7);

        for (i32 i = 4; i <= 6; ++i)
        {
            const auto newIt = array.insert(array.begin() + i - 1, i);
            ASSERT_EQ(*newIt, i);

            ASSERT_TRUE(std::is_sorted(array.begin(), array.end()));
        }

        ASSERT_EQ(array.size(), 10);

        for (i32 i = 0; i < 10; ++i)
        {
            ASSERT_EQ(array[i], i + 1);
        }

        array.insert(array.end(), 11);
        array.insert(array.end(), 12);
        array.insert(array.end(), 13);

        for (i32 i = 0; i < 13; ++i)
        {
            ASSERT_EQ(array[i], i + 1);
        }
    }

    TEST(dynamic_array, dynamic_array_erase_range)
    {
        dynamic_array<i32> array;

        array = {1, 2, 0, 0, 3, 4, 5, 42, 42};

        ASSERT_EQ(array.size(), 9);

        const auto it = array.begin() + 3;
        ASSERT_EQ(*it, 0);

        const auto newIt = array.erase(array.begin() + 2, array.begin() + 4);
        ASSERT_EQ(*newIt, 3);

        ASSERT_EQ(array.size(), 7);

        for (i32 i = 0; i < 5; ++i)
        {
            ASSERT_EQ(array[i], i + 1);
        }

        for (i32 i = 6; i < 7; ++i)
        {
            ASSERT_EQ(array[i], 42);
        }

        const auto endIt = array.erase(array.begin() + 5, array.end());
        ASSERT_EQ(endIt, array.end());

        ASSERT_EQ(array.size(), 5);

        for (i32 i = 0; i < 5; ++i)
        {
            ASSERT_EQ(array[i], i + 1);
        }
    }

    TEST(dynamic_array, dynamic_array_erase_single_element)
    {
        dynamic_array<i32> array;

        array = {1, 2, -1, -1, 3, 4, 5, -1, -1};

        ASSERT_EQ(array.size(), 9);

        u32 iterations{0};

        for (auto it = array.begin(); it != array.end();)
        {
            ++iterations;

            if (*it < 0)
            {
                it = array.erase(it);
            }
            else
            {
                ++it;
            }
        }

        ASSERT_EQ(array.size(), 5);
        ASSERT_EQ(iterations, 9);

        for (i32 i = 0; i < 5; ++i)
        {
            ASSERT_EQ(array[i], i + 1);
        }
    }

    TEST(dynamic_array, dynamic_array_erase_unordered)
    {
        dynamic_array<i32> array;

        array = {1, 2, -1, -1, 3, 4, 5, -1, -1};

        ASSERT_EQ(array.size(), 9);

        u32 iterations{0};

        for (auto it = array.begin(); it != array.end();)
        {
            ++iterations;

            if (*it < 0)
            {
                it = array.erase_unordered(it);
            }
            else
            {
                ++it;
            }
        }

        ASSERT_EQ(array.size(), 5);
        ASSERT_EQ(iterations, 9);

        std::sort(array.begin(), array.end());

        for (i32 i = 0; i < 5; ++i)
        {
            ASSERT_EQ(array[i], i + 1);
        }
    }
}