#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle_pool.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename Tag, typename Value>
    struct handle;

    template <typename Tag, typename T, u32 GenBits>
    struct pool_flat_key_extractor
    {
        static constexpr T extract_key(const handle<Tag, T> h) noexcept
        {
            return handle_pool<T, GenBits>::get_index(h.value);
        }

        static consteval T invalid_key() noexcept
        {
            return u32{};
        }
    };

    template <typename Tag, typename Value, typename T, u32 GenBits>
    class handle_flat_pool_dense_map :
        private flat_dense_map<handle<Tag, T>, Value, pool_flat_key_extractor<Tag, T, GenBits>>
    {
    public:
        using base = flat_dense_map<handle<Tag, T>, Value, pool_flat_key_extractor<Tag, T, GenBits>>;

        using typename base::extractor_type;
        using typename base::key_type;
        using typename base::value_type;

        template <typename... Args>
        auto emplace(Args&&... args) noexcept
        {
            const auto key = key_type{m_pool.acquire()};

            const auto [it, inserted] = base::emplace(key, std::forward<Args>(args)...);

            if (!inserted)
            {
                m_pool.release(key.value);
                return std::pair{it, key_type{}};
            }

            return std::pair{it, key};
        }

        auto erase(key_type key)
        {
            if (base::erase(key) != 0)
            {
                m_pool.release(key.value);
            }
        }

        using base::try_find;

    private:
        handle_pool<T, GenBits> m_pool;
    };

    template <typename Tag, typename Value>
    using h32_flat_pool_dense_map = handle_flat_pool_dense_map<Tag, Value, u32, 4>;

    template <typename Tag, typename Value>
    using h64_flat_pool_dense_map = handle_flat_pool_dense_map<Tag, Value, u64, 8>;
}