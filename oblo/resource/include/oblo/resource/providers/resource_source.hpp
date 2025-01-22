#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/type_id.hpp>

namespace oblo
{
    using load_resource_fn = bool (*)(void* resource, cstring_view source);

    struct loadable_resource
    {
        type_id resourceType;
        string_builder path;
        load_resource_fn load;
    };

    class resource_source
    {
    public:
        virtual ~resource_source() = default;

        virtual bool find(loadable_resource& outResult) = 0;
    };
}