#pragma once

namespace oblo
{
    template <typename T>
    struct flat_key_extractor;

    template <typename Key, typename Value, typename KeyExtractor = flat_key_extractor<Key>>
    class flat_dense_map;

    template <typename Key, typename KeyExtractor = flat_key_extractor<Key>>
    class flat_dense_set;
}