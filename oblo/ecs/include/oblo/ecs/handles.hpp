#pragma once

#include <oblo/core/handle.hpp>

namespace oblo::ecs
{
    struct entity_handle;
    struct component_type_handle;
    struct tag_type_handle;

    using entity = h32<entity_handle>;
    using component_type = h32<component_type_handle>;
    using tag_type = h32<tag_type_handle>;

    constexpr u32 entity_generation_bits = 4;
}