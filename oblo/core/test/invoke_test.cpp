#include <gtest/gtest.h>

#include <oblo/core/invoke/function_ref.hpp>

namespace oblo
{
    TEST(function_ref, function_pointer_return_void)
    {
        function_ref<void(int*)> f;
        ASSERT_FALSE(f);

        void (*call)(int*) = [](int* p) { *p = 42; };

        f = call;
        ASSERT_TRUE(f);

        int res{};

        f(&res);

        ASSERT_EQ(res, 42);
    }

    TEST(function_ref, function_pointer_return_int)
    {
        function_ref<int(int)> f;
        ASSERT_FALSE(f);

        int (*call)(int) = [](int p) { return p * 2; };

        f = call;
        ASSERT_TRUE(f);

        const int res = f(21);

        ASSERT_EQ(res, 42);
    }

    TEST(function_ref, function_lambda_return_void)
    {
        function_ref<void(int*)> f;
        ASSERT_FALSE(f);

        f = [](int* p) { *p = 42; };
        ASSERT_TRUE(f);

        int res{};

        f(&res);

        ASSERT_EQ(res, 42);
    }

    TEST(function_ref, function_lambda_return_int)
    {
        function_ref<int(int)> f;
        ASSERT_FALSE(f);

        f = [](int p) { return p * 2; };
        ASSERT_TRUE(f);

        const int res = f(21);

        ASSERT_EQ(res, 42);
    }
}