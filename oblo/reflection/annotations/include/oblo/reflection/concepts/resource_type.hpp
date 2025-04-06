#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo::reflection
{
    struct resource_type
    {
        type_id typeId;
        uuid typeUuid;
    };
}