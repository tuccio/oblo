#pragma once

#include <oblo/core/type_id.hpp>

namespace oblo
{
    class cstring_view;

    using create_resource_fn = void* (*) ();
    using destroy_resource_fn = void (*)(void*);
    using load_resource_fn = bool (*)(void* resource, cstring_view source);
    using save_resource_fn = bool (*)(const void* resource, cstring_view destination);

    struct resource_type_descriptor
    {
        type_id type;
        create_resource_fn create;
        destroy_resource_fn destroy;
        load_resource_fn load;
        save_resource_fn save;
    };
}