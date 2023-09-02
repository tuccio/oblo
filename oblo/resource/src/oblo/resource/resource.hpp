#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/resource/resource_type_desc.hpp>

namespace oblo
{
    struct resource
    {
        void* data;
        type_id type;
        std::atomic<u32> counter;
        destroy_resource_fn destroy;
    };

    inline resource* resource_create(void* data, type_id type, destroy_resource_fn destroy)
    {
        return new resource{
            .data = data,
            .type = type,
            .counter = 0,
            .destroy = destroy,
        };
    }

    inline void resource_release(resource* resource)
    {
        if (resource->counter.fetch_sub(1, std::memory_order::release) == 1)
        {
            resource->destroy(resource->data);
            delete resource;
        }
    }

    inline void resource_acquire(resource* resource)
    {
        resource->counter.fetch_add(1);
    }
}