#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

namespace oblo::ecs
{
    struct tag_type_desc
    {
        type_id type;
        uuid stableId;
    };
}