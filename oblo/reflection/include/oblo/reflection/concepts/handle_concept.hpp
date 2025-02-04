#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <type_traits>

namespace oblo::reflection
{
    struct handle_concept
    {
        type_id tagType;
        type_id valueType;
    };

    template <typename>
    struct is_handle : std::false_type
    {
    };

    template <typename T, typename V>
    struct is_handle<handle<T, V>> : std::true_type
    {
    };

    template <typename Handle>
        requires(is_handle<Handle>::value)
    handle_concept make_handle_concept()
    {
        return {
            .tagType = get_type_id<typename Handle::tag_type>(),
            .valueType = get_type_id<typename Handle::value_type>(),
        };
    }
}