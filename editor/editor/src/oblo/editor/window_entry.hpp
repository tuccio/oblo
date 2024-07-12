#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/editor/service_context.hpp>

#include <memory_resource>
#include <string_view>

namespace oblo
{
    class service_registry;
}

namespace oblo::editor
{
    struct window_update_context;

    using memory_pool = std::pmr::unsynchronized_pool_resource;

    using update_fn = bool (*)(u8*, const window_update_context& ctx);
    using destroy_fn = void (*)(memory_pool& pool, u8*);

    struct window_entry
    {
        u8* ptr;
        update_fn update;
        destroy_fn destroy;
        service_context services;
        window_entry* parent;
        window_entry* firstChild;
        window_entry* prevSibling;
        window_entry* firstSibling;
        type_id typeId;
        std::string_view debugName;
    };
}