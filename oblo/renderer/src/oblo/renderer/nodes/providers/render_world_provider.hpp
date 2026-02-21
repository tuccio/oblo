#pragma once

#include <oblo/renderer/data/render_world.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct render_world_provider
    {
        data<render_world> inOutRenderWorld;
    };
}