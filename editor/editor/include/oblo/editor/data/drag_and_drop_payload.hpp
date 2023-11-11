#pragma once

#include <oblo/core/lifetime.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo::editor
{
    struct drag_and_drop_payload
    {
        alignas(long double) char data[64];
    };

    namespace payloads
    {
        constexpr const char* Resource{"oblo::resource"};

        constexpr drag_and_drop_payload pack_uuid(const uuid& id)
        {
            drag_and_drop_payload p;
            *start_lifetime_as<uuid>(p.data) = id;;
            return p;
        }

        constexpr uuid parse_uuid(const drag_and_drop_payload& payload)
        {
            return *start_lifetime_as<uuid>(payload.data);
        }

        constexpr uuid parse_uuid(const void* payload)
        {
            return parse_uuid(*start_lifetime_as<drag_and_drop_payload>(payload));
        }
    }
}