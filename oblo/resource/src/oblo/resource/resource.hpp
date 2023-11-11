#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/resource/type_desc.hpp>

namespace oblo
{
    struct resource
    {
        void* data;
        type_id type;
        std::string name;
        std::atomic<u32> counter;
        destroy_resource_fn destroy;
    };

    namespace detail
    {
        resource* resource_create(void* data, type_id type, std::string name, destroy_resource_fn destroy);
        void resource_release(resource* resource);
        void resource_acquire(resource* resource);
    }
}