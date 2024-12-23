#pragma once

#include <oblo/core/expected.hpp>

namespace oblo
{
    namespace ecs
    {
        class entity_registry;
    }

    class data_document;
    class property_registry;

    namespace ecs_serializer
    {
        struct write_config
        {
        };

        SCENE_API expected<> write(data_document& doc,
            u32 docRoot,
            const ecs::entity_registry& reg,
            const property_registry& propertyRegistry,
            const write_config& cfg = {});

        SCENE_API expected<> read(ecs::entity_registry& reg, const data_document& doc);
    }
}