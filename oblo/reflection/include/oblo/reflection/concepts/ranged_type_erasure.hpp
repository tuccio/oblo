#pragma once

#include <oblo/core/types.hpp>

namespace oblo::reflection
{
    using ranged_create_fn = void (*)(void* dst, usize count);
    using ranged_destroy_fn = void (*)(void* dst, usize count);
    using ranged_move_fn = void (*)(void* dst, void* src, usize count);
    using ranged_move_assign_fn = void (*)(void* dst, void* src, usize count);

    struct ranged_type_erasure
    {
        ranged_create_fn create;
        ranged_destroy_fn destroy;
        ranged_move_fn move;
        ranged_move_assign_fn moveAssign;
    };

    template <typename T>
    ranged_type_erasure make_ranged_type_erasure()
    {
        ranged_type_erasure rte{
            .destroy =
                [](void* ptr, usize count)
            {
                T* const begin = static_cast<T*>(ptr);
                T* const end = begin + count;

                for (T* it = begin; it != end; ++it)
                {
                    it->~T();
                }
            },
        };

        if constexpr (requires { T{}; })
        {
            rte.create = [](void* dst, usize count)
            { std::uninitialized_value_construct_n(static_cast<T*>(dst), count); };
        }

        if constexpr (requires(T&& other) { T{std::move(other)}; })
        {
            rte.move = [](void* dst, void* src, usize count)
            {
                T* outIt = static_cast<T*>(dst);
                T* inIt = static_cast<T*>(src);
                T* const end = outIt + count;

                for (; outIt != end; ++outIt, ++inIt)
                {
                    new (outIt) T(std::move(*inIt));
                }
            };
        }

        if constexpr (requires(T& lhs, T&& rhs) { lhs = std::move(rhs); })
        {
            rte.moveAssign = [](void* dst, void* src, usize count)
            {
                T* outIt = static_cast<T*>(dst);
                T* inIt = static_cast<T*>(src);
                T* const end = outIt + count;

                for (; outIt != end; ++outIt, ++inIt)
                {
                    *outIt = std::move(*inIt);
                }
            };
        }

        return rte;
    }
}