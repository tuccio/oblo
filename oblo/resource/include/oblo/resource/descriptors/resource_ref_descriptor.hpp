#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo
{
    struct resource_ref_descriptor
    {
        type_id typeId;
        uuid typeUuid;
    };
}