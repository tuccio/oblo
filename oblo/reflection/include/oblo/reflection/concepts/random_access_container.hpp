#pragma once

#include <oblo/core/concepts/random_access_container.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <type_traits>

namespace oblo::reflection
{
    using random_access_container_size_fn = usize (*)(void* dst);
    using random_access_container_at_fn = void* (*) (void* dst, usize index);
    using random_access_container_resize_fn = void (*) (void* dst, usize size);

    struct random_access_container
    {
        type_id valueType;
        random_access_container_size_fn size;
        random_access_container_at_fn at;
        random_access_container_resize_fn optResize;
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
        }

        rac.at = [](void* dst, usize index) -> void* { return &(*static_cast<T*>(dst))[index]; };

        if constexpr (requires(T c, usize s) { c.resize(s); })
        {
            rac.optResize = [](void* dst, usize size) { return static_cast<T*>(dst)->resize(size); };
        }

        return rac;
    }
}