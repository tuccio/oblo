#include <gtest/gtest.h>

#include <oblo/core/dynamic_array.hpp>
#include <oblo/thread/job_manager.hpp>
#include <oblo/thread/parallel_for.hpp>

#include <thread>

namespace oblo
{
    TEST(job_manager, init_shutdown)
    {
        job_manager jm;

        ASSERT_TRUE(jm.init());
        jm.shutdown();
    }

    TEST(job_manager, basic_waitable)
    {
        job_manager jm;

        ASSERT_TRUE(jm.init());

        std::atomic<int> value{};

        const auto j = jm.push_waitable([&value] { value = 42; });

        jm.wait(j);

        ASSERT_EQ(value, 42);

        jm.shutdown();
    }

    TEST(job_manager, waitable_child)
    {
        job_manager jm;

        ASSERT_TRUE(jm.init());

        constexpr u32 N{4096};

        std::atomic<int> value{};

        const auto root = jm.push_waitable([&value] {});

        std::atomic<int> destructionsCounter{};

        struct destruction_counter
        {
            std::atomic<int>* leakCheck;

            destruction_counter(std::atomic<int>* leakCheck) : leakCheck{leakCheck}
            {
                ++(*leakCheck);
            }

            destruction_counter(const destruction_counter&) = delete;
            destruction_counter(destruction_counter&& other) noexcept : leakCheck{other.leakCheck}
            {
                ++(*leakCheck); // Ideally we can delete the move instead
            }

            ~destruction_counter()
            {
                --(*leakCheck);
            }
        };

        // Push N children, each with 2 children
        for (auto i = 0; i < N; ++i)
        {
            jm.push_child(root,
                [&jm, &value, &destructionsCounter, onDestruction = destruction_counter{&destructionsCounter}](
                    const job_context& ctx)
                {
                    ++value;
                    jm.push_child(ctx.job,
                        [&value, onDestruction = destruction_counter{&destructionsCounter}] { ++value; });
                    jm.push_child(ctx.job,
                        [&value, onDestruction = destruction_counter{&destructionsCounter}] { ++value; });
                });
        }

        jm.wait(root);

        ASSERT_EQ(value, 3 * N);

        jm.shutdown();

        ASSERT_EQ(destructionsCounter, 0);
    }

    TEST(job_manager, non_copiable_functor)
    {
        job_manager jm;

        ASSERT_TRUE(jm.init());

        std::atomic<int> value{};

        struct functor
        {
            functor(std::atomic<int>* value) : value{value} {}
            functor(const functor&) = delete;
            functor(functor&&) = default;

            functor& operator=(const functor&) = delete;
            functor& operator=(functor&&) = delete;

            void operator()() const
            {
                *value = 42;
            }

            std::atomic<int>* value{};
        };

        const auto j = jm.push_waitable(functor{&value});

        jm.wait(j);

        ASSERT_EQ(value, 42);

        jm.shutdown();
    }

    TEST(parallel_for, basic_iteration)
    {
        job_manager jm;

        ASSERT_TRUE(jm.init());

        constexpr u32 N{1 << 16u};

        dynamic_array<int> values;

        values.resize(N);

        parallel_for(
            [&values](job_range range)
            {
                for (u32 i = range.begin; i < range.end; ++i)
                {
                    values[i] = i;
                }
            },
            job_range{0, u32(values.size())},
            32);

        ASSERT_EQ(values.size(), N);

        for (u32 i = 0; i < N; ++i)
        {
            ASSERT_EQ(values[i], i);
        }

        jm.shutdown();
    }

    TEST(parallel_for, basic_iteration_2d)
    {
        job_manager jm;

        ASSERT_TRUE(jm.init());

        static constexpr u32 N{128u};

        static constexpr u32 rowsGran{2};
        static constexpr u32 colsGran{4};

        i8 matrix[N][N]{};

        parallel_for_2d(
            [&matrix](job_range rows, job_range cols)
            {
                ASSERT_EQ(rows.end - rows.begin, rowsGran);
                ASSERT_EQ(cols.end - cols.begin, colsGran);

                for (u32 i = rows.begin; i < rows.end; ++i)
                {
                    for (u32 j = cols.begin; j < cols.end; ++j)
                    {
                        matrix[i][j] = 42;
                    }
                }
            },
            job_range{0, N},
            job_range{0, N},
            rowsGran,
            colsGran);

        jm.shutdown();

        for (u32 i = 0; i < N; ++i)
        {
            for (u32 j = 0; j < N; ++j)
            {
                ASSERT_EQ(matrix[i][j], 42) << "i: " << i << " j: " << j;
            }
        }

        //jm.shutdown();
    }
}