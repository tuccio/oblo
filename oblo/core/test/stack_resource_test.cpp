#include <gtest/gtest.h>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/small_vector.hpp>
#include <oblo/core/stack_allocator.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    TEST(stack_allocator, stack)
    {
        constexpr u32 N{1024};

        for (u32 i = 1; i < N; ++i)
        {
            constexpr auto stackSize = sizeof(u32) * N;

            stack_only_allocator<stackSize, alignof(u32)> stack;
            dynamic_array<u32> vec{&stack};
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
            constexpr auto stackSize = sizeof(u32) * N;

            stack_fallback_allocator<stackSize, alignof(u32)> stack;
            dynamic_array<u32> vec{&stack};
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