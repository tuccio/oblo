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
        static constexpr u32 key_bits = sizeof(T) * 8 - GenBits;
        static constexpr u32 generation_bits = GenBits;

        static constexpr T extract_key(const handle<Tag, T> h) noexcept
        {
            return handle_pool<T, GenBits>::get_index(h.value);
        }

        static consteval T invalid_key() noexcept
        {
            return T{};
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
            OBLO_ASSERT(inserted, "Handles should be available if they are in the pool");

            if (!inserted) [[unlikely]]
            {
                m_pool.release(key.value);
                return std::pair{it, key_type{}};
            }

            return std::pair{it, key};
        }

        u32 erase(key_type key)
        {
            const auto res = base::erase(key);

            if (res != 0)
            {
                m_pool.release(key.value);
            }

            return res;
        }

        using base::at;
        using base::try_find;

        using base::keys;
        using base::values;

        using base::reserve_dense;
        using base::reserve_sparse;

        using base::clear;

        using base::size;

        using base::empty;

    private:
        handle_pool<T, GenBits> m_pool;
    };

    template <typename Tag, typename Value = Tag, u32 GenBits = 4>
    using h32_flat_pool_dense_map = handle_flat_pool_dense_map<Tag, Value, u32, GenBits>;

    template <typename Tag, typename Value = Tag, u32 GenBits = 8>
    using h64_flat_pool_dense_map = handle_flat_pool_dense_map<Tag, Value, u64, GenBits>;

    template <typename Tag, typename Value = Tag, u32 GenBits = 4>
    using h32_flat_extpool_dense_map =
        flat_dense_map<handle<Tag, u32>, Value, pool_flat_key_extractor<Tag, u32, GenBits>>;

    template <typename Tag, typename Value = Tag, u32 GenBits = 8>
    using h64_flat_extpool_dense_map =
        flat_dense_map<handle<Tag, u64>, Value, pool_flat_key_extractor<Tag, u64, GenBits>>;
}