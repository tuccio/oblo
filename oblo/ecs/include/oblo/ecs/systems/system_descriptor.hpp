#pragma once

#include <oblo/core/type_id.hpp>
#include <string_view>

namespace oblo::ecs
{
    struct system_update_context;

    using system_create_fn = void* (*) ();
    using system_destroy_fn = void (*)(void*);
    using system_update_fn = void (*)(void* system, const system_update_context* ctx);

    struct system_descriptor
    {
        std::string_view name;
        type_id typeId;
        system_create_fn create;
        system_destroy_fn destroy;
        system_update_fn firstUpdate;
        system_update_fn update;
    };
}