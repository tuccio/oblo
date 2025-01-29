#include <oblo/resource/resource_ptr.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/resource.hpp>
#include <oblo/thread/job_manager.hpp>

namespace oblo::detail
{
    resource* resource_create(const resource_type_descriptor* desc, uuid id, string_view name, string_view path)
    {
        return new resource{
            .id = id,
            .name = name.as<string>(),
            .path = path.as<string>(),
            .descriptor = desc,
            .counter = 0,
        };
    }

    void resource_release(resource* resource)
    {
        if (resource->counter.fetch_sub(1, std::memory_order::release) == 1)
        {
            if (resource->loadJob)
            {
                // We could wait at any time after it's done loading, but this is as good of a moment as any other,
                // except these jobs might be living for a long time after they are completed
                job_manager::get()->wait(resource->loadJob);
            }

            if (resource->data)
            {
                resource->descriptor->destroy(resource->data);
            }

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
        return resource->descriptor->typeId;
    }

    string_view resource_name(resource* resource)
    {
        return string_view{resource->name};
    }

    uuid resource_uuid(resource* resource)
    {
        return resource->id;
    }

    void resource_start_loading(resource* resource)
    {
        resource_load_state expected{resource_load_state::unloaded};

        if (resource->loadState.compare_exchange_strong(expected, resource_load_state::loading))
        {
            OBLO_ASSERT(!resource->loadJob);

            // Acquire the resource here so we don't have to worry about it being deleted while loading
            resource_acquire(resource);

            resource->loadJob = job_manager::get()->push_waitable(
                [resource]
                {
                    OBLO_ASSERT(!resource->data);
                    resource->data = resource->descriptor->create();

                    // TODO: We should handle load failure somehow, instead of handing a half baked resource
                    resource->descriptor->load(resource->data, resource->path);

                    resource->loadState.store(resource_load_state::loaded);
                    resource_release(resource);
                });
        }
    }

    bool resource_is_loaded(resource* resource)
    {
        return resource->loadState.load() == resource_load_state::loaded;
    }

    void resource_load_sync(resource* resource)
    {
        resource_start_loading(resource);
        OBLO_ASSERT(resource->loadJob);

        auto* const jm = job_manager::get();
        jm->increase_reference(resource->loadJob);
        jm->wait(resource->loadJob);
    }
}