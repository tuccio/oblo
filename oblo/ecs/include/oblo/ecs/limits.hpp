#pragma once

#include <oblo/core/types.hpp>

namespace oblo::ecs
{
    // We use 254 because we use as invalid, and this limit simplifies handling of masks in type_set
    static constexpr u32 MaxComponentTypes {254};
    static constexpr u32 MaxTagTypes {MaxComponentTypes};
}