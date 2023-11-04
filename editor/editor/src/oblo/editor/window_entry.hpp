#pragma once

#include <oblo/core/types.hpp>

#include <memory_resource>

namespace oblo
{
    class service_registry;
}

namespace oblo::editor
{
    using memory_pool = std::pmr::unsynchronized_pool_resource;

    using update_fn = bool (*)(u8*, const window_update_context& ctx);
    using destroy_fn = void (*)(memory_pool& pool, u8*);

    struct window_entry
    {
        u8* ptr;
        update_fn update;
        destroy_fn destroy;
        service_registry* services;
        window_entry* parent;
        window_entry* firstChild;
        window_entry* prevSibling;
        window_entry* firstSibling;
        bool isServiceRegistryOwned;
    };
}