#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo::ecs
{
    using create_fn = void (*)(void* dst, usize count);
    using destroy_fn = void (*)(void* dst, usize count);
    using move_fn = void (*)(void* dst, void* src, usize count);
    using move_assign_fn = void (*)(void* dst, void* src, usize count);

    struct component_type_desc
    {
        type_id type;
        uuid stableId;
        u32 size;
        u32 alignment;
        create_fn create;
        destroy_fn destroy;
        move_fn move;
        move_assign_fn moveAssign;
    };
}