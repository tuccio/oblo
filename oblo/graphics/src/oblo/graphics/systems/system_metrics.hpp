#pragma once

#include <oblo/core/types.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct draw_registry_metrics
    {
        usize totalInstances;

        OBLO_PROPERTY(Bytes)
        usize totalInstanceDataSize;
    } OBLO_REFLECT();
}