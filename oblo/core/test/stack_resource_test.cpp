#include <gtest/gtest.h>

#include <oblo/core/small_vector.hpp>
#include <oblo/core/stack_allocator.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    // Stack tests may fail in debug, e.g. due to Microsoft STL allocating for iterator debugging
    TEST(stack_allocator, stack)
    {
        constexpr u32 N{1024};

        for (u32 i = 1; i < N; ++i)
        {
            constexpr auto stackSize = sizeof(u32) * N;

            const stack_allocator<stackSize, alignof(u32)> stack;
            std::pmr::vector<u32> vec{stack};
            vec.reserve(N);

            vec.resize(i, i);

            for (auto value : vec)
            {
                ASSERT_EQ(i, value);
            }

            auto* dataPtr = reinterpret_cast<const char*>(vec.data());
            auto* stackPtr = reinterpret_cast<const char*>(std::addressof(stack));

            ASSERT_LT(std::abs(dataPtr - stackPtr), std::ptrdiff_t(N));
        }
    }

    TEST(stack_allocator, heap)
    {
        constexpr u32 N{1024};

        for (u32 i = N + 1; i < 2 * N; ++i)
        {
            const stack_allocator<sizeof(u32) * N, alignof(u32)> stack;
            std::pmr::vector<u32> vec{stack};
            vec.reserve(N);

            vec.resize(i, i);

            for (auto value : vec)
            {
                ASSERT_EQ(i, value);
            }

            auto* dataPtr = reinterpret_cast<const char*>(vec.data());
            auto* stackPtr = reinterpret_cast<const char*>(std::addressof(stack));

            ASSERT_GE(std::abs(dataPtr - stackPtr), std::ptrdiff_t(N));
        }
    }

    TEST(small_vector, stack)
    {
        constexpr u32 N{1024};

        for (u32 i = 1; i < N; ++i)
        {
            small_vector<u32, N> vec;
            vec.resize(i, i);

            for (auto value : vec)
            {
                ASSERT_EQ(i, value);
            }

            auto* dataPtr = reinterpret_cast<const char*>(vec.data());
            auto* stackPtr = reinterpret_cast<const char*>(std::addressof(vec));

            ASSERT_LT(std::abs(dataPtr - stackPtr), std::ptrdiff_t(N));
        }
    }

    TEST(small_vector, heap)
    {
        constexpr u32 N{1024};

        for (u32 i = N + 1; i < 2 * N; ++i)
        {
            small_vector<u32, N> vec;
            vec.resize(i, i);

            for (auto value : vec)
            {
                ASSERT_EQ(i, value);
            }

            auto* dataPtr = reinterpret_cast<const char*>(vec.data());
            auto* stackPtr = reinterpret_cast<const char*>(std::addressof(vec));

            ASSERT_GE(std::abs(dataPtr - stackPtr), std::ptrdiff_t(N));
        }
    }
}