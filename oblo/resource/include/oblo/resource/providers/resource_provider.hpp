#pragma once

#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct resource_added_event
    {
        uuid id;
        uuid typeUuid;
        string_view name;
        cstring_view path;
    };

    struct resource_removed_event
    {
        uuid id;
    };

    struct resource_updated_event
    {
        uuid id;
        uuid typeUuid;
        string_view name;
        cstring_view path;
    };

    class resource_provider
    {
    public:
        using on_add_fn = function_ref<void(const resource_added_event& e)>;
        using on_remove_fn = function_ref<void(const resource_removed_event& e)>;
        using on_update_fn = function_ref<void(const resource_updated_event& e)>;

    public:
        virtual ~resource_provider() = default;

        virtual void iterate_resource_events(on_add_fn onAdd, on_remove_fn onRemove, on_update_fn onUpdate) = 0;
    };
}