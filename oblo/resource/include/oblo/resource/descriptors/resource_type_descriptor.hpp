#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    class cstring_view;

    using create_resource_fn = void* (*) ();
    using destroy_resource_fn = void (*)(void*);
    using load_resource_fn = bool (*)(void* resource, cstring_view source);

    struct resource_type_descriptor
    {
        type_id typeId;
        uuid typeUuid;
        create_resource_fn create;
        destroy_resource_fn destroy;
        load_resource_fn load;
    };
}