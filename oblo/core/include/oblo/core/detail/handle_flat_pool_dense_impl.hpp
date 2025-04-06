#pragma once

#include <oblo/core/detail/flat_dense_impl.hpp>
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

    namespace detail
    {
        template <typename Tag, typename Value, typename ValueStorage, typename T, u32 GenBits>
        class handle_flat_pool_dense_impl :
            public flat_dense_impl<handle<Tag, T>, Value, ValueStorage, pool_flat_key_extractor<Tag, T, GenBits>>
        {
        public:
            using base = flat_dense_impl<handle<Tag, T>, Value, ValueStorage, pool_flat_key_extractor<Tag, T, GenBits>>;

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

            auto erase(key_type key)
            {
                const auto res = base::erase(key);

                if (res != 0)
                {
                    m_pool.release(key.value);
                }

                return res;
            }

        private:
            handle_pool<T, GenBits> m_pool;
        };
    }
}