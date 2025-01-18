#include <gtest/gtest.h>
#include <oblo/core/unique_ptr.hpp>

#include <gtest/gtest.h>

namespace oblo
{
    TEST(unique_ptr, construction)
    {
        unique_ptr<int> p1;
        EXPECT_EQ(p1.get(), nullptr);
        EXPECT_FALSE(p1);

        unique_ptr<int> p2 = allocate_unique<int>(42);
        EXPECT_TRUE(p2);
        EXPECT_EQ(*p2, 42);
    }

    TEST(unique_ptr, destruction)
    {
        struct foo
        {
            ~foo()
            {
                *destroyed = true;
            }

            bool* destroyed{};
        };

        bool wasDestroyed = false;

        {
            unique_ptr p = allocate_unique<foo>(&wasDestroyed);
            EXPECT_FALSE(wasDestroyed);
        }

        EXPECT_TRUE(wasDestroyed);
    }

    TEST(unique_ptr, move)
    {
        unique_ptr<int> p1 = allocate_unique<int>(42);
        unique_ptr<int> p2(std::move(p1));
        EXPECT_EQ(p1.get(), nullptr);
        EXPECT_FALSE(p1);
        EXPECT_EQ(*p2, 42);
        EXPECT_TRUE(p2);

        unique_ptr<int> p3;
        p3 = std::move(p2);
        EXPECT_EQ(p2.get(), nullptr);
        EXPECT_EQ(*p3, 42);
    }

    TEST(unique_ptr, accessors)
    {
        unique_ptr<int> p = allocate_unique<int>(42);
        EXPECT_EQ(*p, 42);
        EXPECT_EQ(*p.operator->(), 42);
    }

    TEST(unique_ptr, convert)
    {
        struct foo
        {
            int v;
        };

        struct bar : foo
        {
        };

        unique_ptr<foo> f1 = allocate_unique<bar>(42);
        EXPECT_EQ(f1->v, 42);

        unique_ptr<foo> f2;
        unique_ptr<bar> b1 = allocate_unique<bar>(84);
        f2 = std::move(b1);
        EXPECT_EQ(f2->v, 84);
    }

    TEST(unique_ptr, release_and_reset)
    {
        unique_ptr<int> p = allocate_unique<int>(42);
        int* rawPtr = p.release();
        EXPECT_EQ(p.get(), nullptr);
        EXPECT_EQ(*rawPtr, 42);

        *rawPtr = 84;
        p.reset(rawPtr);

        EXPECT_EQ(*p, 84);
        p.reset();

        EXPECT_EQ(p.get(), nullptr);
    }

    TEST(unique_ptr, functor)
    {
        int foo{0};

        {
            EXPECT_EQ(foo, 0);
            unique_ptr<int, decltype([](int* value) { *value = 42; })> p1;
            EXPECT_EQ(foo, 0);
        }

        {
            EXPECT_EQ(foo, 0);
            unique_ptr<int, decltype([](int* value) { *value = 42; })> p2{&foo};
            EXPECT_EQ(foo, 0);
        }

        EXPECT_EQ(foo, 42);
    }

    TEST(unique_ptr_array, construction)
    {
        unique_ptr<int[]> p1 = allocate_unique<int[]>(5);

        for (int i = 0; i < 5; ++i)
        {
            EXPECT_EQ(p1[i], 0); // Default initialized to zero
        }
    }

    TEST(unique_ptr_array, destruction)
    {
        struct foo
        {
            ~foo()
            {
                ++*counter;
            }

            u32* counter{};
        };

        u32 counter = 0;

        {
            unique_ptr<foo[]> p = allocate_unique<foo[]>(5);

            for (int i = 0; i < 5; ++i)
            {
                p[i].counter = &counter;
            }

            EXPECT_EQ(counter, 0);
        }

        EXPECT_EQ(counter, 5);
    }

    TEST(unique_ptr_array, move)
    {
        unique_ptr<int[]> p1 = allocate_unique<int[]>(5);
        for (int i = 0; i < 5; ++i)
        {
            p1[i] = i;
        }

        unique_ptr<int[]> p2 = std::move(p1);
        EXPECT_EQ(p1, nullptr);
        for (int i = 0; i < 5; ++i)
        {
            EXPECT_EQ(p2[i], i);
        }

        unique_ptr<int[]> p3;
        p3 = std::move(p2);
        EXPECT_EQ(p2, nullptr);
        for (int i = 0; i < 5; ++i)
        {
            EXPECT_EQ(p3[i], i);
        }
    }

    TEST(unique_ptr_array, accessors)
    {
        unique_ptr<int[]> p = allocate_unique<int[]>(5);

        for (int i = 0; i < 5; ++i)
        {
            p[i] = i * 2;
        }

        for (int i = 0; i < 5; ++i)
        {
            EXPECT_EQ(p[i], i * 2);
        }
    }

    TEST(unique_ptr_array, release_and_reset)
    {
        unique_ptr<int[]> p = allocate_unique<int[]>(5);
        for (int i = 0; i < 5; ++i)
        {
            p[i] = i;
        }

        int* rawPtr = p.release();
        EXPECT_EQ(p, nullptr);

        for (int i = 0; i < 5; ++i)
        {
            EXPECT_EQ(rawPtr[i], i);
        }

        p.reset(rawPtr, 5);
        EXPECT_EQ(p, rawPtr);

        p.reset();
        EXPECT_EQ(p, nullptr);

        p = allocate_unique<int[]>(5);

        for (int i = 0; i < 5; ++i)
        {
            p[i] = i * 3;
        }

        for (int i = 0; i < 5; ++i)
        {
            EXPECT_EQ(p[i], i * 3);
        }

        p.reset();
        EXPECT_EQ(p, nullptr);
    }

}
