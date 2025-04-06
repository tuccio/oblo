#pragma once

#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/handle_flat_pool_set.hpp>
#include <oblo/ecs/handles.hpp>

namespace oblo::ecs
{
    template <typename Value>
    using entity_map = h32_flat_extpool_dense_map<entity_handle, Value, entity_generation_bits>;

    using entity_set = h32_flat_extpool_dense_set<entity_handle, entity_generation_bits>;
}