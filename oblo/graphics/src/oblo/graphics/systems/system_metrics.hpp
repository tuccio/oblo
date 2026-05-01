#pragma once

#include <oblo/core/types.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    struct draw_registry_metrics
    {
        u64 totalInstances;

        OBLO_PROPERTY(Bytes)
        u64 totalInstanceDataSize;

        OBLO_PROPERTY(Bytes)
        u64 meshTablesSize;
    } OBLO_REFLECT();
}