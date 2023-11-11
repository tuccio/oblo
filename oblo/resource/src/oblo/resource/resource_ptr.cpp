#include <oblo/resource/resource_ptr.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/resource/resource.hpp>

namespace oblo::detail
{
    resource* resource_create(void* data, type_id type, std::string name, destroy_resource_fn destroy)
    {
        return new resource{
            .data = data,
            .type = type,
            .name = std::move(name),
            .counter = 0,
            .destroy = destroy,
        };
    }

    void resource_release(resource* resource)
    {
        if (resource->counter.fetch_sub(1, std::memory_order::release) == 1)
        {
            resource->destroy(resource->data);
            delete resource;
        }
    }

    void resource_acquire(resource* resource)
    {
        resource->counter.fetch_add(1);
    }

    void* resource_data(resource* resource)
    {
        return resource->data;
    }

    type_id resource_type(resource* resource)
    {
        return resource->type;
    }

    std::string_view resource_name(resource* resource)
    {
        return resource->name;
    }
}