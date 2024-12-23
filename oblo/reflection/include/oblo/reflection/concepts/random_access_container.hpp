#pragma once

#include <oblo/core/concepts/random_access_container.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <type_traits>

namespace oblo::reflection
{
    using random_access_container_size_fn = usize (*)(void* dst);
    using random_access_container_at_fn = void* (*) (void* dst, usize index);

    struct random_access_container
    {
        type_id valueType;
        random_access_container_size_fn size;
        random_access_container_at_fn at;

        /// @brief Only set for contiguous containers.
        random_access_container_at_fn optData;
    };

    template <oblo::random_access_container T>
    random_access_container make_random_access_container()
    {
        random_access_container rac{};

        if constexpr (std::is_bounded_array_v<T>)
        {
            rac.valueType = get_type_id<std::remove_all_extents_t<T>>();
            rac.size = [](void*) -> usize { return std::extent_v<T, 0>; };
        }
        else
        {
            rac.valueType = get_type_id<typename T::value_type>();
            rac.size = [](void* dst) -> usize { return static_cast<T*>(dst)->size(); };

            if constexpr (requires(T& c) { c.data(); })
            {
                rac.optData = [](void* dst, usize index) -> void* { return static_cast<T*>(dst)->data(index); };
            }
        }

        rac.at = [](void* dst, usize index) -> void* { return &(*static_cast<T*>(dst))[index]; };

        return rac;
    }
}