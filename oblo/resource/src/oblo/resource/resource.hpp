#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>

#include <atomic>

namespace oblo
{
    struct resource
    {
        void* data;
        type_id type;
        uuid id;
        string name;
        std::atomic<u32> counter;
        destroy_resource_fn destroy;
    };

    namespace detail
    {
        resource* resource_create(void* data, type_id type, uuid id, string name, destroy_resource_fn destroy);
        void resource_release(resource* resource);
        void resource_acquire(resource* resource);
    }
}