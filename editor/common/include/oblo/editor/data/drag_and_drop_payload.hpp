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
        constexpr const char* Artifact{"oblo::artifact"};
        constexpr const char* Asset{"oblo::asset"};

        namespace detail
        {
            inline drag_and_drop_payload pack_uuid(const uuid& id)
            {
                drag_and_drop_payload p;
                *start_lifetime_as<uuid>(p.data) = id;
                return p;
            }

            inline uuid parse_uuid(const drag_and_drop_payload& payload)
            {
                return *start_lifetime_as<uuid>(payload.data);
            }

        }

        inline drag_and_drop_payload pack_asset(const uuid& id)
        {
            return detail::pack_uuid(id);
        }

        inline drag_and_drop_payload pack_artifact(const uuid& id)
        {
            return detail::pack_uuid(id);
        }

        inline uuid parse_artifact(const void* payload)
        {
            return detail::parse_uuid(*start_lifetime_as<drag_and_drop_payload>(payload));
        }

        inline uuid parse_asset(const void* payload)
        {
            return detail::parse_uuid(*start_lifetime_as<drag_and_drop_payload>(payload));
        }
    }
}