#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle_flat_pool_set.hpp>
#include <oblo/core/handle_pool.hpp>
#include <oblo/core/types.hpp>

namespace oblo
{
    template <typename Tag, typename Value, typename T, u32 GenBits>
    class handle_flat_pool_dense_map : detail::handle_flat_pool_dense_impl<Tag, Value, dynamic_array<Value>, T, GenBits>
    {
        using base_type = detail::handle_flat_pool_dense_impl<Tag, Value, dynamic_array<Value>, T, GenBits>;

    public:
        using key_type = base_type::key_type;
        using extractor_type = base_type::extractor_type;

        using base_type::base_type;
        using base_type::operator=;

        using base_type::at;
        using base_type::clear;
        using base_type::emplace;
        using base_type::empty;
        using base_type::erase;
        using base_type::keys;
        using base_type::reserve_dense;
        using base_type::reserve_sparse;
        using base_type::size;
        using base_type::size32;
        using base_type::try_find;

        std::span<const Value> values() const
        {
            return base_type::value_storage();
        }

        std::span<Value> values()
        {
            return base_type::value_storage();
        }
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