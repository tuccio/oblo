#pragma once

#include <oblo/core/type_id.hpp>

namespace oblo::ecs
{
    using create_fn = void (*)(void* dst, usize count);
    using destroy_fn = void (*)(void* dst, usize count);
    using move_fn = void (*)(void* dst, const void* src, usize count);

    struct component_type_desc
    {
        type_id type;
        u32 size;
        u32 alignment;
        create_fn create;
        destroy_fn destroy;
        move_fn move;
    };
}