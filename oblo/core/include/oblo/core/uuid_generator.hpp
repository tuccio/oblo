#pragma once

#include <oblo/core/hash.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/uuid.hpp>

#include <random>
#include <span>

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

    class uuid_namespace_generator
    {
    public:
        uuid_namespace_generator() = default;

        explicit uuid_namespace_generator(const uuid& name)
        {
            std::memcpy(m_name, &name, sizeof(uuid));
        }

        uuid_namespace_generator(const uuid_namespace_generator&) = default;
        uuid_namespace_generator& operator=(const uuid_namespace_generator&) = default;

        uuid generate(std::span<const byte> data) const
        {
            return generate(string_view{reinterpret_cast<const char*>(data.data()), data.size()});
        }

        uuid generate(string_view data) const
        {
            constexpr auto hasher = hash<string_view>{};
            const usize hash = hasher(data);

            usize res[N];

            for (usize i = 0; i < N; ++i)
            {
                res[i] = hash_mix(m_name[i], hash);
            }

            return std::bit_cast<uuid>(res);
        }

    private:
        static constexpr usize N{sizeof(uuid) / sizeof(usize)};
        usize m_name[N]{};
    };
}