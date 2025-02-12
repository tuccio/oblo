#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/thread/job_manager.hpp>

#include <atomic>

namespace oblo
{
    enum class resource_load_state : u8
    {
        unloaded,
        loading,
        loaded,
    };

    struct resource
    {
        void* data;
        uuid id;
        string name;
        string path;
        job_handle loadJob{};
        const resource_type_descriptor* descriptor;
        std::atomic<u32> counter;
        std::atomic<resource_load_state> loadState;
    };

    namespace detail
    {
        resource* resource_create(const resource_type_descriptor* desc, uuid id, string_view name, string_view path);
        void resource_release(resource* resource);
        void resource_acquire(resource* resource);
        bool resource_instantiate(resource* resource);
        void resource_start_loading(resource* resource);
        bool resource_is_loaded(resource* resource);
    }
}