#pragma once

#include <oblo/core/handle.hpp>

namespace oblo::graphics
{
    using ViewportImageId = void*;

    struct viewport_component
    {
        u32 width;
        u32 height;
        ViewportImageId imageId;
    };
}