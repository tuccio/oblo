#pragma once

#include <oblo/core/uuid.hpp>

#include <random>

namespace oblo
{
    class uuid_random_generator
    {
    public:
        uuid_random_generator() : m_rng{std::random_device{}()} {}

        uuid_random_generator(const uuid_random_generator&) = delete;
        uuid_random_generator(uuid_random_generator&&) noexcept = default;

        uuid_random_generator& operator=(const uuid_random_generator&) = delete;
        uuid_random_generator& operator=(uuid_random_generator&&) noexcept = default;

        uuid generate()
        {
            std::uniform_int_distribution<u64> dist{};
            const u64 bytes[2]{dist(m_rng), dist(m_rng)};

            return std::bit_cast<uuid>(bytes);
        }

    private:
        std::mt19937_64 m_rng;
    };
}