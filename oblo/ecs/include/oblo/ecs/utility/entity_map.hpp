#pragma once

#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/ecs/handles.hpp>

namespace oblo::ecs
{
    template <typename Value>
    using entity_map = h32_flat_extpool_dense_map<entity_handle, Value, entity_generation_bits>;
}