#pragma once

#include <oblo/core/type_id.hpp>

#include <filesystem>

namespace oblo::resource
{
    using create_resource_fn = void* (*) ();
    using destroy_resource_fn = void (*)(void*);
    using load_resource_fn = bool (*)(void* resource, const std::filesystem::path& source);
    using save_resource_fn = bool (*)(const void* resource, const std::filesystem::path& destination);

    struct type_desc
    {
        type_id type;
        create_resource_fn create;
        destroy_resource_fn destroy;
        load_resource_fn load;
        save_resource_fn save;
    };
}