#pragma once

#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/nodes/shadows/shadow_output.hpp>

namespace oblo::vk
{
    void shadow_output::build(const frame_graph_build_context& ctx)
    {
        const auto& cfg = ctx.access(inConfig);

        ctx.push(outShadowSink, {outShadow, cfg.lightIndex});
    }
}