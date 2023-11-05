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
}