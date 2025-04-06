#pragma once

#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/flat_dense_set.hpp>

#include <span>

namespace oblo
{
    template <typename Key, typename Value, typename KeyExtractor>
    class flat_dense_map : detail::flat_dense_impl<Key, Value, dynamic_array<Value>, KeyExtractor>
    {
        using base_type = detail::flat_dense_impl<Key, Value, dynamic_array<Value>, KeyExtractor>;

    public:
        using key_type = Key;
        using value_type = Value;
        using extractor_type = KeyExtractor;

        using base_type::flat_dense_impl;
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
}