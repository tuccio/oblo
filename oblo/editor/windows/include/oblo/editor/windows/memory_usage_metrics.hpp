#pragma once

#include <oblo/core/types.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo::editor
{
    struct memory_usage_metrics
    {
        OBLO_PROPERTY(Bytes)
        usize ram;

        OBLO_PROPERTY(Bytes)
        usize vramTextureResources;
    } OBLO_REFLECT();
}