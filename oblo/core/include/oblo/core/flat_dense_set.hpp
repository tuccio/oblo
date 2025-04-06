#pragma once

#include <oblo/core/detail/flat_dense_impl.hpp>
#include <oblo/core/flat_dense_forward.hpp>

namespace oblo
{
    template <typename Key, typename KeyExtractor>
    class flat_dense_set : detail::flat_dense_impl<Key, std::nullptr_t, detail::null_value_storage, KeyExtractor>
    {
        using base_type = detail::flat_dense_impl<Key, std::nullptr_t, detail::null_value_storage, KeyExtractor>;

    public:
        using key_type = Key;
        using extractor_type = KeyExtractor;

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
}