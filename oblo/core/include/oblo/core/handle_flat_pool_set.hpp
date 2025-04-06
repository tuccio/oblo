#pragma once

#include <oblo/core/detail/handle_flat_pool_dense_impl.hpp>
#include <oblo/core/flat_dense_set.hpp>

namespace oblo
{
    template <typename Tag, typename T, u32 GenBits>
    class handle_flat_pool_dense_set :
        detail::handle_flat_pool_dense_impl<Tag, std::nullptr_t, detail::null_value_storage, T, GenBits>
    {
        using base_type =
            detail::handle_flat_pool_dense_impl<Tag, std::nullptr_t, detail::null_value_storage, T, GenBits>;

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
    };

    template <typename Tag, u32 GenBits = 4>
    using h32_flat_pool_dense_set = handle_flat_pool_dense_set<Tag, u32, GenBits>;

    template <typename Tag, u32 GenBits = 8>
    using h64_flat_pool_dense_set = handle_flat_pool_dense_set<Tag, u64, GenBits>;

    template <typename Tag, u32 GenBits = 4>
    using h32_flat_extpool_dense_set = flat_dense_set<handle<Tag, u32>, pool_flat_key_extractor<Tag, u32, GenBits>>;

    template <typename Tag, u32 GenBits = 8>
    using h64_flat_extpool_dense_set = flat_dense_set<handle<Tag, u64>, pool_flat_key_extractor<Tag, u64, GenBits>>;
}