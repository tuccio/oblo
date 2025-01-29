#pragma once

#include <oblo/core/deque.hpp>

namespace oblo
{
    struct resource_type_descriptor;

    SCENE_API void fetch_scene_resource_types(deque<resource_type_descriptor>& outResourceTypes);
}