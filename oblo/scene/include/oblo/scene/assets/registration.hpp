#pragma once

#include <oblo/core/dynamic_array.hpp>

namespace oblo
{
    struct resource_type_desc;

    SCENE_API void fetch_scene_resource_types(dynamic_array<resource_type_desc>& outResourceTypes);
}