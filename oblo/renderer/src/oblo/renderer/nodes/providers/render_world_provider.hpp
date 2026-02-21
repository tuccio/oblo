#pragma once

#include <oblo/vulkan/data/render_world.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo
{
    struct render_world_provider
    {
        data<render_world> inOutRenderWorld;
    };
}