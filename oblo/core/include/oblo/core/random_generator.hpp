#pragma once

#include <oblo/core/types.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <limits>

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

        u32 seed();
        u32 seed(u32 seed);

        u32 generate();

        u32 operator()();

        static constexpr u32 min();
        static constexpr u32 max();

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };

    template <typename T>
    struct uniform_distribution
    {
        T min = std::numeric_limits<T>::lowest();
        T max = std::numeric_limits<T>::max();

        T generate(random_generator& gen) const;

        T operator()(random_generator& gen) const
        {
            return generate(gen);
        }
    };

    constexpr u32 random_generator::min()
    {
        return 0u;
    }

    constexpr u32 random_generator::max()
    {
        return ~0u;
    }
}