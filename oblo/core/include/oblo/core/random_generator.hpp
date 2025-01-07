#pragma once

#include <oblo/core/types.hpp>

#include <memory>

namespace oblo
{
    class random_generator
    {
    public:
        using result_type = u32;

    public:
        random_generator();
        random_generator(const random_generator&) = delete;
        random_generator(random_generator&&) noexcept;
        ~random_generator();

        random_generator& operator=(const random_generator&) = delete;
        random_generator& operator=(random_generator&&) noexcept;

        void seed();
        void seed(u32 seed);

        u32 generate();

        u32 operator()();

        static u32 min();
        static u32 max();

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;
    };

    template <typename T>
    struct uniform_distribution
    {
        T min;
        T max;

        T generate(random_generator& gen) const;

        T operator()(random_generator& gen) const
        {
            return generate(gen);
        }
    };
}