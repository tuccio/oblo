#pragma once

#include <oblo/core/lifetime.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/ecs/handles.hpp>

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
        constexpr const char* Entity{"oblo::entity"};

        namespace detail
        {
            template <typename T>
                requires((alignof(T) <= alignof(drag_and_drop_payload)) && std::is_trivially_copyable_v<T>)
            inline drag_and_drop_payload pack(const T& v)
            {
                drag_and_drop_payload p;
                new (p.data) T{v};
                return p;
            }

            template <typename T>
                requires((alignof(T) <= alignof(drag_and_drop_payload)) && std::is_trivially_copyable_v<T>)
            inline T unpack(const drag_and_drop_payload& payload)
            {
                return *start_lifetime_as<T>(payload.data);
            }
        }

        inline drag_and_drop_payload pack_asset(const uuid& id)
        {
            return detail::pack(id);
        }

        inline drag_and_drop_payload pack_artifact(const uuid& id)
        {
            return detail::pack(id);
        }

        inline drag_and_drop_payload pack_entity(const ecs::entity& id)
        {
            return detail::pack(id);
        }

        inline uuid unpack_artifact(const void* payload)
        {
            return detail::unpack<uuid>(*start_lifetime_as<drag_and_drop_payload>(payload));
        }

        inline uuid unpack_asset(const void* payload)
        {
            return detail::unpack<uuid>(*start_lifetime_as<drag_and_drop_payload>(payload));
        }

        inline ecs::entity unpack_entity(const void* payload)
        {
            return detail::unpack<ecs::entity>(*start_lifetime_as<drag_and_drop_payload>(payload));
        }
    }
}